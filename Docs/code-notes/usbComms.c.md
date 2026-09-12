# usbComms.c notes

The longer comments from `usbComms.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope


Reference credit on some of the excellent G2 comms protocol work by
Bruno Verhue in his Delphi editor application:

https://www.bverhue.nl/g2dev/

## 2. file scope

Bank upload (backup) response scratch state — populated by parse_bank_upload_data/
parse_bank_upload_empty on the USB thread, consumed by backup_bank() immediately
after send_bank_upload_request() returns (single-threaded round trip, no locking needed).
Allocated lazily (see ensure_bank_scratch_buffer()) and kept for the rest of the session —
most sessions never touch Bank Backup/Restore at all, so this keeps 20MB (2 x
PATCH_FILE_SIZE) off the app's permanent baseline footprint for everyone else.

## 3. `ensure_bank_scratch_buffer()`

Lazily allocates a PATCH_FILE_SIZE scratch buffer on first use and keeps it for the rest of
the session (freeing/reallocating on every backup or restore run isn't worth the complexity
for state that's reused every time this feature runs again). Returns NULL (and logs) on
allocation failure; callers must check before using the buffer.

## 4. in `ensure_bank_scratch_buffer()`

Parsed-but-not-yet-applied Synth Settings Restore staging area — peek_synth_settings_restore()
fills this in immediately before setting gSynthRestorePeekComplete; apply_synth_settings_restore()
copies it into the live gSynthSettings once the user has confirmed (single-threaded round trip
through the USB thread, same reasoning as the scratch state above).

## 5. `tNameTableEntry()`

WHERE parse_list_names_response() DEPOSITS WHAT IT READS. It points at the live tables for the
one-entry responses that come back from a Store, and at a staging pair while a background sweep
is running — see name_sweep_begin(). The sweep must not write into the live tables as it goes:
the picker is drawn from them, and they hold either the previous run's cached names or the
previous sweep's, so filling them in location by location would mean a Load dialogue opened
during the refresh showed a bank that empties and refills over eight seconds.

## 6. in `name_tables_edited()`

Set around send_store_patch()'s send_and_receive() call — Store's ack reuses this exact
one-entry response format (see parse_list_names_response()'s comment), but it is not a List
Names update: store_patch_to_bank() already updates gPatchNameTable/gPerfNameTable directly from
the edit buffer, and any trailing bytes in the ack past that one entry are not a real bank
continuation, so parsing them as one would fabricate phantom entries. This suppresses just the
name-table writes for that call, harmlessly leaving the rest of parse_list_names_response's
bookkeeping (sListNamesMode/NextBank/NextLoc) alone since nothing consults it for a Store ack.

## 7. `note_usb_activity()`

A lamp timestamp on its own does not put a lamp on screen. The render loop only draws when a
redraw has been REQUESTED, and when nothing else is going on it is asleep in glfwWaitEvents() —
so stamping gUsbRxTime and stopping there meant the Rx indicator changed when the mouse moved
and not when data arrived. Wake the loop, but ONLY on the dark->lit transition: the G2's
interrupt stream runs at around twenty packets a second and a lamp stays lit for COMMS_LAMP_MS,
so waking on every packet would be waking the UI twenty times a second for a lamp that is
already on. Putting one OUT is the render loop's own job — see comms_lamp_state().

Single writer: both timestamps are written only from the USB thread, so the read-then-write here
is not the read-modify-write hazard that _Atomic does not protect against.

## 8. `open_and_claim_device()`

Opens the G2 and claims interface 0. Returns true on success.
On macOS: no kernel driver detach needed — libusb uses IOKit directly.
libusb_reset_device on macOS triggers USBDeviceReEnumerate, which resets
the bulk endpoint DATA0/DATA1 toggle bits — without it the host and device
can be out of phase after a reconnect, causing all transfers to time out.

## 9. `usb_transfer_cb()`

---------------------------------------------------------------------------
Async-backed synchronous transfer — replaces libusb_bulk_transfer.
Drives the libusb event loop in 50ms slices so the wall-clock timeout is
reliable even when the macOS IOKit backend ignores per-transfer timeouts.
---------------------------------------------------------------------------

## 10. in `parse_param_change()`

The Parameter Pages panel is a live readout of whichever params its page's knobs are
assigned to, and turning one of those knobs on the G2 itself arrives here - so it needs a
wake, where the canvas has always got away without one. Deliberately not filtered down to
"is this param actually on the page being shown": that would mean resolving eight knob
assignments on the USB thread for every param change the device reports.

## 11. in `parse_volume_indicator()`

A MULTI-BIT LED GROUP TAKES ONE ENTRY OUT OF THIS STREAM, not one 2-bit value per
LED out of the 0x39 one. 8Counter, BinCounter, ADConv, the three Mux modules and
FlipFlop are all of this shape: several LEDs driven by a single value whose BITS
are the LEDs. Consuming them from the other stream cost the modules after them
eight slots each, which is why LEDs looked right until a patch contained one.

## 12. in `parse_volume_indicator()`

The two top flag bits say the value IS a bit set; they are spread
a bit at a time only when both are present (and only for groups under twelve LEDs,
which all of these are). Anything else is some other encoding we have not had
to decode, so show nothing rather than show nonsense.

## 13. `parse_led_data()`

LED (blink) data, sub-command 0x39. The wire format: after the sub-command byte comes a start index, then
packed 2-bit values, FOUR TO A BYTE AND LOWEST BITS FIRST —

```
    led[n + 0] = byte & 3        led[n + 2] = (byte >> 4) & 3
    led[n + 1] = (byte >> 2) & 3 led[n + 3] = (byte >> 6)

```
which is what the indexing below does, directly. It used to reverse each data byte in place and
then read the pairs back MSB-first through read_bit_stream(). That put the VALUES in the right
order but transposed the two bits WITHIN each one, and render_led_common()'s colour mapping was
the mirror image of that, so the two cancelled and the LEDs came out right. Both now follow the wire format
directly, which draws exactly the same pixels and leaves neither half depending on
the other being wrong. It also stops us writing into the receive buffer.

The index space is the whole slot, VA then FX, one index per LED with a module's LEDs
consecutive: a module appears once per LED group, and its repeats give the index within the
module.

LED_STREAM_SIZE (40) is where it ENDS, not how much one message holds: the original walks from the
start index to 0x28 and stops, and UpdateBlink() gives each area Min(itsLedCount, 0x28 - used), so
40 is the whole index space for both areas together and a patch with more LEDs than that has the
surplus unreported by the instrument. A message therefore covers startIndex..LED_STREAM_SIZE-1 —
it is NOT startIndex + 40.
Returns true if any LED it stored DIFFERS from the one already there — same reason as
parse_volume_indicator() above.

## 14. `RESOURCES_RECORD_BYTES`

One record is a fixed 29 bytes: sub(1) location(1) cyclesRed(2) cyclesBlue(2) zpMem(1)
unknown(2) xmemV1(2) ymemV1(2) pmemV1(2) xmemV2(2) ymemV2(2) pmemV2(2) ram(2) qmem(4) rmem(2).
The guard has to require the WHOLE record: asking only that a byte remains lets a truncated
message read past the end, which is the mismatch that made parse_midi_cc() silently wrong.

## 15. `parse_list_names_response()`

Reads a SUB_COMMAND_LIST_NAMES (0x14) response — reverse-engineered from a real startup capture
(StartupCapture.pcapng, see project memory): [4 bytes unknown][mode][0x03 marker][bank]
[location], then repeating [Clavia name][category:1] entries in ascending location order
(a sparse listing — unpopulated locations are simply never sent, not represented by a
placeholder). If the current bank's real content runs out before the packet does, a literal
0x03 byte followed by a 2-byte [newBank][newLocation] pair appears inline and more entries
continue for the new bank. A short (6-byte) response — just [4 unknown][mode][0x04], no bank/
location/entries — means that whole domain (patch or performance) is exhausted.

This subcommand (0x13) is the same value as SUB_RESPONSE_STORE_PATCH — Store's ack turns out to
be a one-entry instance of this exact format (confirmed by decoding a captured Store ack against
this same layout), so no separate handler is needed for it; send_store_patch()'s caller doesn't
consult the scratch state this fills in, so the (harmless) reuse costs nothing.

Leaves the result in sListNamesMode/sListNamesFinished (this response's outcome) and
sListNamesNextBank/sListNamesNextLoc (where send_list_names_sweep() should resume next, when not
finished) — see globalVars.h's gPatchNameTable/gPerfNameTable for where entries themselves land.

## 16. in `parse_list_names_response()`

entry always begins with a byte GREATER THAN 5 — the low six values are
the control vocabulary (0x01 JUMP, 0x02 SKIP, 0x03 BANK, 0x04 MODE,
0x05 CONTINUE). This used to test for 0x03 alone, which was enough in
practice because 0x03 is the only one that ever appears mid-stream and a
trailing 0x05 was always the last byte — but "always the last byte" is a
property of the responses we happen to have seen, and any control byte
with two bytes behind it would have been read as the start of a Clavia
string and desynced the rest of the response. Checked before every entry,
not just after the loop exhausts the bank, because a sparse bank can end
well before location 128.

## 17. in `parse_list_names_response()`

COPY_STRING, NOT strncpy. strncpy pads with zeros only when the source is
SHORTER than n: at exactly CLAVIA_NAME_SIZE it writes 16 bytes and no
terminator, and the reader then runs on into whatever follows the field. That
is how "distant activity" — exactly 16 characters — came to be displayed in the
Load Patch picker as "distant activity Ringmod Basses". The tables are globals
so the byte is zero at startup, which is why it only showed up once an entry
had been rewritten by a re-sweep, a Store or a Delete.

## 18. in `parse_list_names_response()`

ANYTHING ELSE MEANS "RESUME FROM (bank, location)". The control-code vocabulary is
0x01 JUMP (next byte is a location), 0x02 SKIP one slot, 0x03 BANK (the two bytes above),
0x04 MODE (switch patch/performance) and 0x05 CONTINUE — named in JanBurp's independent
reverse-engineering of this protocol (NordModularG2-Editor, docs/technical/usb-protocol.md).
MEASURED 2026-08-29 against the connected G2: EVERY response of a full sweep ends 0x05 and
no other code is ever seen here, so the others are documented rather than implemented —
handling codes the instrument does not send would be untestable guesswork.

0x05 is also the whole story behind the bank-boundary spin that name_sweep_step() rolls
over: CONTINUE says "there is more, resume from where you got to", and at the end of a
bank where you got to is location 128 — which is not a location, so asking for it returns
the same answer for ever. The device does not emit an 0x03 to cross a bank when the
crossing falls on a response boundary.

## 19. `parse_bank_upload_data()`

Wire format (reverse-engineered from real Bank Upload captures, both Patch and Performance
domains — identical framing in both cases): after the
[responseType][commandResponse][version][subCommand] header already consumed by the
caller: 1 byte echoing the request's domain (BANK_UPLOAD_DOMAIN_PATCH/_PERFORMANCE), 1
reserved byte, 1 location byte (0-indexed, echoes the request), a Clavia name (up to
CLAVIA_NAME_SIZE bytes, null-terminated unless it fills all 16 — see read_clavia_string), a
16-bit length L, a 2-byte echoed [version][type] marker (discarded — the real one is repeated
at the start of the content that follows), then L-1 raw bytes that are byte-identical to a
.pch2/.prf2 file's own binary body (confirmed by full byte-for-byte diffs of captured responses
against real sample files of both types) — safe to write to disk as-is. Performance responses
have 2 further trailing bytes after that (likely an outer CRC) that are simply never read,
same as they would be for Patch if present.

## 20. `param_list_header_is_plausible()`

The device's own SUB_RESPONSE_PARAM_LIST (0x4d), arriving as a message rather than as a section of
a patch dump. The PAYLOAD is the same one parse_param_list() already reads — location, module
count, variation count, then the values — but WHETHER A 16-BIT SECTION LENGTH SITS IN FRONT OF IT
IS NOT KNOWN, and the two handlers in this file disagree on that question for messages of exactly
this shape: SUB_RESPONSE_GLOBAL_KNOBS reads a length first, SUB_RESPONSE_KNOBS does not. Inside a
patch dump there is always one (parse_patch() consumes it before dispatching: type, then a 16-bit length, for every section,
0x4d included).

Guessing is not free here: parse_param_list() writes a value into EVERY variation of every module
it walks, so a header read at the wrong offset silently rewrites the whole patch's knob values
rather than failing. So test the header before trusting it, at both candidate offsets, and refuse
to parse if neither looks like one. The first real capture then answers the question in the log
instead of leaving it open.

## 21. in `param_list_header_is_plausible()`

location 0/1 are the two areas, 2 is the patch-settings context; the variation count is 10 live
(9 in a file, which cannot reach this path but costs nothing to accept). An EMPTY section is
written as 0 modules and 0 variations and is perfectly valid — it also parses to nothing
whichever offset it is read at, so accepting it costs nothing either.

## 22. `tParamListFraming`

Which way the last 0x4d went. The answer is worth ONE line, not one per message: the point of it is
to settle the framing the first time the device sends one, and a run that gets a stream of them
would otherwise bury the rest of the log. A CHANGE is always announced, so a device that sends both
shapes cannot hide behind the first one.

## 23. `parse_command_response()`

wantsRedraw comes in true and is only ever cleared: a message is assumed to have changed
something until a parser says otherwise. Only the two continuously-pushed streams are in a
position to say so — everything else here is a response to something we asked for, or an
unsolicited report of a real event, and is worth a frame on arrival.

## 24. in `parse_command_response()`

AND THE SLOT MATTERS AS MUCH AS THE VALUE. The G2 meters and blinks every slot that is
running, but render_modules() draws gSlot alone, so a change in another slot's LEDs is a
change to something nothing is looking at. The values are stored either way — switching
slot asks for its own frame and finds them already current.

## 25. in `parse_command_response()`

SUB_COMMAND_SET_PARAM_MODE (0x3e, incoming) and SUB_RESPONSE_PERF_HEADER (0x11) parsers were
removed 2026-07-26: hardware capture confirmed the device never sends either (perf-mode
changes arrive via SUB_RESPONSE_PERF_PATCH_VERSIONS 0x1f, perf state via 0x29). Both sub-codes
are still used on the SEND side (send_perf_mode_change / send_perf_header). If the device ever
did send one it now falls through to the default unhandled-message log below.

## 26. in `parse_incoming()`

WAKE ON A CHANGE, NOT ON ARRIVAL. This was unconditional, and with a G2 attached the LED and
volume streams arrive around twenty times a second forever — so the editor redrew twenty
times a second while sitting untouched in front of a patch whose meters were not moving. The
parsers now report whether they actually changed a pixel's worth of state, and a patch with
nothing blinking costs no frames at all. One that IS blinking still redraws at the rate the
instrument reports it, which is the point.

## 27. `send_bank_upload_request()`

Requests a single (bank, location) slot during a Bank Upload (backup). domain selects Patch
(BANK_UPLOAD_DOMAIN_PATCH) vs Performance (BANK_UPLOAD_DOMAIN_PERFORMANCE) — reverse-engineered
from a Performance Bank Upload capture: identical framing to the Patch case, with this one byte
switched from 0x00 to 0x01 (confirmed against 29 sample slots + a byte-for-byte diff of the
decoded content against real .prf2 files). Response lands in sBankUploadContent/sBankUploadName
(via parse_bank_upload_data) or sBankUploadGotData is left false if the slot is empty (via
parse_bank_upload_empty) — both are accepted as success by int_rec's special-cased termination
check for SUB_COMMAND_PATCH_BANK_DATA.

## 28. `send_bank_clear()`

Erases one Bank Restore location (SUB_COMMAND_CLEAR, 0x0c) — reverse-engineered from a real
restore capture (PatchRestore.pcapng): domain, bank, location, then a trailing byte that was
constant 0x01 across all 126 samples in the capture (meaning unconfirmed, hardcoded here).
Acked with SUB_RESPONSE_CLEAR (0x15). Used for every location in the target bank that the
restore folder doesn't have a file for, so the bank ends up matching the folder exactly
rather than being merged into — this is the "erase" the stock editor warns about.

## 29. `send_bank_download_push()`

Pushes one Bank Restore location's content (SUB_COMMAND_PATCH_BANK_DATA, 0x19) — the exact same
message shape as a Bank Upload response, just sent host->device instead of device->host (same
capture as send_bank_clear above). Fields: domain, bank, location, Clavia name, a 16-bit length
(contentLen + 1, mirroring backup's "L-1" the other way), a 2-byte [version][type] marker that
echoes content's own first 2 bytes, then the raw .pch2/.prf2 body verbatim. Acked with
SUB_RESPONSE_PATCH_BANK_UPLOAD (0x18) — the same code Bank Upload uses for "location empty";
here it just means "write accepted". name is best-effort (stripped of any backup-collision
"(2)" suffix) — the name that actually sticks is the one embedded in content itself.

## 30. `send_store_patch()`

Commits whatever patch is currently loaded in the active edit-buffer slot to bank/location on the
device (SUB_COMMAND_STORE, 0x0b) — reverse-engineered from a real capture
(SaveEditBufferToBank7-1.pcapng): domain, bank, location, no patch content — the device already
has the patch in its edit buffer, so there's nothing to transmit. Which edit-buffer slot is
implicit (the device's own current focus, tracked separately via SUB_COMMAND_SELECT_SLOT) —
there's no slot field in this message. Acked with SUB_RESPONSE_STORE_PATCH (0x13), routed in
parse_command_response to parse_list_names_response (same value as SUB_RESPONSE_LIST_NAMES,
see that function's comment) — sSuppressNameTableUpdate is set around the call below so that
ack doesn't get misread as a List Names update (store_patch_to_bank() updates the name-table
cache directly instead, from the edit buffer it just told the device to store).

## 31. in `send_store_patch()`

First payload byte is the target SLOT, not the bank-upload "domain" the original code sent
there — that value is 0 for patches (== slot A), which is exactly why Store/Load always hit
slot A regardless of selection. Confirmed via a stock-editor RETRIEVE (load) capture:
"01 2c 41 0a 01 00 00" loads bank1/loc1 into slot B; STORE (0x0b) is the mirror operation in
the same command family and uses the same framing (inferred by symmetry — not yet confirmed
with a stock-editor store capture, since store is a flash write).

THE PERFORMANCE IS ADDRESSED AS SLOT MAX_SLOTS. It used to send the bank-upload domain value
here, which is 1 — and since this byte is a slot, that asked the G2 for SLOT B rather than
for the performance. parse_patch_version() already decodes MAX_SLOTS as the perf version
rather than a slot's, so that index is how this protocol names the performance.

## 32. `send_retrieve_patch()`

Loads bank/location into a specific edit-buffer slot (SUB_COMMAND_RETRIEVE, 0x0a). The target
slot is the FIRST payload byte — confirmed against a real stock-editor capture of loading a patch
into slot B: "01 2c 41 0a 01 00 00" (system-addressed, RETRIEVE, then slot=01 bank=00 loc=00).
The original code sent the bank-upload "domain" there (0 for patches == slot A), which is why it
always loaded into slot A. Acked with SUB_RESPONSE_PATCH_VERSION_CHANGE (0x38) — already fully
handled: parse_patch_version_change() sets gotPatchChangeIndication[slot] and state_handler()'s
loop pulls the new patch in and refreshes the UI (same path as a front-panel patch change).

## 33. in `send_retrieve_patch()`

A performance load changes all four slots at once, and the G2 says so with
SUB_RESPONSE_PERF_PATCH_VERSIONS rather than the single-slot SUB_RESPONSE_PATCH_VERSION_CHANGE.
Waiting for the patch-domain reply made a working load look like a failure — and worse, the
retry re-sent the RETRIEVE, so the performance was loaded three times over before giving up.

## 34. `peek_bank_location()`

Reads back just the name (and whether it's populated) of whatever currently occupies bank/
location — for showing an overwrite warning before Store, without needing any new wire protocol:
reuses the already-confirmed Bank Upload request/response path (send_bank_upload_request),
discarding the content and keeping only sBankUploadName/sBankUploadGotData.

## 35. `build_unique_backup_filename()`

Fills outName with "baseName.<ext>", or "baseName(2).<ext>", "baseName(3).<ext>", etc. if a
file by that name already exists in destFolder. Patch/performance names are free text on the
hardware and often repeat across (or even within) banks, so this keeps a same-named item from
a different slot from silently overwriting an earlier backup. The device write path never sees
the suffix — the name that matters is embedded in the file content itself (see read_clavia_string
in protocol.c), so restore logic can ignore it entirely.

## 36. `backup_bank()`

Loops every location in a Patch or Performance Bank (isPerf selects which), writing each
populated slot to destFolder as a .pch2/.prf2 file plus a .pchList manifest matching the real
Nord editor's own bank-dump format ("Version=Nord Modular G2 Bank Dump", confirmed identical
for both domains against a captured PerfBank1.pchList sample). Read-only against the connected
G2 — never touches the live in-memory slot/patch state.

Performance framing was reverse-engineered from a real capture: request/response are identical
to the Patch case (same subcommands 0x17/0x19/0x18) with the domain byte switched to
BANK_UPLOAD_DOMAIN_PERFORMANCE. One difference from Patch responses: there are 2 extra bytes
after the L-1-byte content (likely an outer CRC) — harmless, since only contentLength bytes are
ever copied out. Not confirmed from the capture: the empty-slot response (0x18) — every one of
the 29 sampled locations was populated, so this assumes it matches the Patch case exactly.
silent suppresses this bank's own completion popup/flag and leaves gBankBackupActive set on
return — used by backup_everything() to chain many banks under one continuous progress dialog
and a single final summary alert instead of one popup per bank.

## 37. `parse_bank_manifest()`

Reads a "PatchBankN.pchList"/"PerfBankN.pchList" manifest (as written by backup_bank() above)
into a per-location filename table for restore_bank() below. Lines are "bank:location: filename"
(CRLF-terminated, 1-indexed on both fields, as written by backup_bank()); only lines matching
sourceBank1Indexed are kept. fileNames[location] is left as an empty string for any location the
manifest doesn't mention (or if the manifest can't be opened at all) — restore_bank() erases
every one of those, same as the stock editor does for an unpopulated location.
Returns false if manifestPath couldn't be opened at all — the caller must treat that as a hard
failure rather than "every location happens to be empty": an all-empty fileNames table is
otherwise indistinguishable from a genuinely empty bank, and restore_bank() would silently erase
every location in destBank instead of refusing to proceed.

## 38. `restore_bank()`

Restores a Bank Backup folder onto the connected G2 — the mirror of backup_bank() above. Every
one of NUM_LOCATIONS_PER_BANK locations in destBank is acted on: a location the manifest has a
file for gets that file pushed (send_bank_download_push); everything else gets erased
(send_bank_clear). destBank therefore ends up exactly matching the source folder rather than
being merged into — confirmed from a real restore capture (PatchRestore.pcapng) where the stock
editor did exactly this (pushed the 2 populated locations it had files for, then cleared the
other 126 in the bank). sourceBank is the bank the manifest was recorded against (used to build
the manifest filename and to filter its lines); destBank is where the write actually goes —
normally the same bank ("restore to where it came from") but callers may pass a different value
for a cross-bank restore, since nothing in the wire protocol ties push/clear to a particular bank
beyond the explicit bank byte in each message. Unlike backup_bank(), a failure partway through
stops immediately rather than continuing best-effort — this is writing to the device, so the
caller needs to know exactly how far it got. silent mirrors backup_bank()'s chaining support, for
a future restore_everything().

## 39. in `restore_bank()`

Keep the name-table cache in sync immediately — same reasoning as
store_patch_to_bank()/delete_bank_location() above, otherwise every location this
restore just wrote keeps showing its old (or empty) contents in Load/Store/Delete
pickers until the next full reconnect sweep. Unlike Store, there's no live edit
buffer to read the category from, so peek_patch_category() pulls it straight out of
the pushed content itself.

## 40. `peek_store_target()`

Looks up what's currently at bank/location (Patch or Performance domain, matching whichever the
edit buffer is currently in — gGlobalSettings.perfMode, read by the caller) so the UI can warn
before overwriting it. Reuses the already-confirmed Bank Upload wire path via
peek_bank_location() — no content is kept, just the name and populated flag. Result lands in
gStorePeek* globals, polled by check_action_flags() in graphics.cpp. Performance-domain Store
itself is assumed by the same domain-byte symmetry already relied on for Performance Bank
Restore/Delete — not independently captured.

## 41. `store_patch_to_bank()`

Commits the current edit-buffer patch/performance to bank/location on the device via
send_store_patch(), once the user has confirmed past the overwrite warning peek_store_target()
set up. Result is posted to the UI via post_alert_response() (reverse queue), drained by
check_action_flags() in graphics.cpp.

## 42. in `store_patch_to_bank()`

Keep the name-table cache in sync immediately — otherwise a picker built from it (Load/
Store/Delete) would keep showing the old contents of this location until the next full
reconnect sweep. The name/category just written are whatever's in the current edit
buffer (that's what Store just sent), no device round-trip needed to know them.

## 43. `peek_delete_target()`

Looks up what's currently at bank/location (Patch or Performance domain) before Delete, same
reasoning and mechanism as peek_store_target() above — reuses the Bank Upload read path via
peek_bank_location(), no new wire protocol. Result lands in gDeletePeek* globals, polled by
check_action_flags() in graphics.cpp.

## 44. `delete_bank_location()`

Erases bank/location (Patch or Performance domain) via SUB_COMMAND_CLEAR — the same request
send_bank_clear() already makes internally for every unpopulated location during restore_bank(),
here exposed directly as a standalone user-facing delete. The Patch-domain framing was confirmed
from a real capture (see [[bank-backup-protocol]]); Performance-domain CLEAR is assumed by the
same domain-byte symmetry already relied on for Performance Bank Restore, not independently
captured. Result is posted to the UI via post_alert_response() (reverse queue).

## 45. `peek_load_target()`

Looks up what's currently at bank/location before Load — same mechanism as peek_store_target()/
peek_delete_target() (reuses peek_bank_location(), no new wire protocol). Here the point isn't an
overwrite warning about the target (Load doesn't touch it) but (a) letting the user confirm this
is really the patch/performance they meant to load, and (b) warning that loading replaces
whatever's currently in the edit buffer, which could be unsaved. Result lands in gLoadPeek*
globals, polled by check_action_flags() in graphics.cpp.

## 46. `load_patch_from_bank()`

Loads bank/location into the edit buffer via send_retrieve_patch(), once the user has confirmed
past the peek_load_target() warning. Failure is posted to the UI via post_alert_response() — the
actual patch content arriving into the editor happens separately/automatically via the existing
patch-change-notification path (see send_retrieve_patch's comment), not something this function
needs to wait for.

## 47. in `load_patch_from_bank()`

A performance load replaces the perf's own settings — its NAME above all — and nothing else
fetches them. parse_perf_patch_versions() raises gotPerfSettingsChangeIndication only when the
perf VERSION changes, and a load does not necessarily change it: the log of a working load
reads "Old perf = 8 new = 8", so the refresh never fired and the previous performance's name
stayed on screen.

Safe to read here, unlike the file-load path (load_perf_from_payload), which deliberately does
NOT do this: there the settings are about to be overwritten from the file, so reading the
device would pull the OLD perf's data over them. Here the device IS the source.

## 48. `build_synth_settings_backup_filename()`

Builds "synth-<h>.<mm><am|pm>-<dd><Mon><yy>.txt", e.g. "synth-2.34pm-25Jun26.txt" for 2:34pm on
25 June 2026. Deliberately avoids ':' in the timestamp — Finder/HFS's legacy handling of colons
in filenames makes them unsafe, the same reason patch names get sanitized in
build_unique_backup_filename() above.

## 49. `backup_synth_settings()`

Snapshots gSynthSettings (populated by send_get_synth_settings(), below) to a timestamped,
plain-text key:value file. Unlike Bank Upload there's no Clavia wire format for this — it's a
house format for reviewing instrument-wide config (MIDI/sysex/tuning/pedal/etc.) alongside
patch and performance backups, not something restorable via the .pch2/.pchList path.
silent suppresses this call's own completion popup/flag — used by backup_everything() so only
a single final summary alert fires for the whole sweep.

## 50. `find_latest_synth_settings_backup()`

Scans folder for "synth-*.txt" backup files (see build_synth_settings_backup_filename() above)
and returns the most recently modified one by filesystem mtime in outFilePath — simpler and just
as reliable as parsing the "2.34pm-25Jun26" timestamp back out of the filename, since mtime
already reflects when the file was written.

## 51. `parse_synth_settings_backup_file()`

Reverse of backup_synth_settings()'s fprintf lines above — reads the house key:value text format
back into outSettings. Rejects anything that doesn't start with the expected "Version=" header
(not a recognizable Synth Settings Backup); unrecognized keys are otherwise ignored rather than
failing, so a file from a slightly different version of this house format still restores what it
can.

## 52. in `parse_synth_settings_backup_file()`

Same unterminated-at-exactly-16 hazard as the name tables: this destination belongs to
the caller and is not guaranteed to have been zeroed first. (The two other strncpy
name copies in this file write into locals declared = {0}, so their terminator is
there by construction.)

## 53. `peek_synth_settings_restore()`

Finds and parses the latest Synth Settings Backup in folder (pure local file I/O, no device
round trip — still runs on the USB thread rather than directly from the UI thread, for
consistency with every other multi-step device action in this file). Leaves the parsed result in
sSynthSettingsRestoreStaged for apply_synth_settings_restore() below once the user confirms.
Result lands in gSynthRestorePeek* globals, polled by check_action_flags() in graphics.cpp.

## 54. `backup_everything()`

Runs a full Patch Bank + Performance Bank + Synth Settings backup in one sweep, using the
per-item functions above in "silent" mode so only a single combined summary alert fires at the
end instead of one popup per bank. gBankBackupActive/IsPerf/Bank/Location/Written stay driven
by whichever backup_bank() call is currently running, so the existing progress dialog keeps
updating continuously across the whole sweep; gBankBackupIsEverything just changes its title.

## 55. in `send_set_param_label()`

A PARAMETER MAY CARRY MORE THAN ONE NAME. It always could — the wire format is a COUNT of
7-byte names per parameter, and write_param_names() has always written them all — but nothing
used more than the first until Channel Select radio buttons, which are one parameter with one
name per button. Sending only name 0 told the instrument an 8-button group had one caption.

## 56. `send_write_cable_colour()`

Recolours an EXISTING cable in place. This is NOT
interchangeable with re-sending send_write_cable() at the new colour: the G2 treats a write as
an ADD, so a rewrite leaves the patch holding the same cable twice, once per colour. The
editor's own write_cable() coalesces the two, so nothing looks wrong until the patch is read
back — and a later delete then removes only one of the copies. Confirmed from a real capture:
a patch built by two drag-connects came back from the device with four cables (2026-08-01).

Header byte is (location << 3) | colour: same field positions as send_write_cable()'s, minus
its 0x10 flag (the part-of-dump bit), which recolour has no equivalent of.

## 57. `clear_slot_data_usb()`

The USB thread's own slot clear, NOT dataBase.c's clear_slot_data(). The two are deliberately
kept apart because they do not agree: this one leaves gMorphCount at 8 where the other zeroes it,
and it does not clear gNote2/gPatchNotes at all. Which is right is an open question — until it is
answered, neither path's behaviour is changed by merging them. They were previously both called
clear_slot_data(), the static one here quietly shadowing the other.

## 58. `push_slot_to_device()`

---------------------------------------------------------------------------
Patch upload helper — builds and sends one slot to the device.
Synth must already be stopped before calling.
Consumes the SUB_RESPONSE_PATCH_VERSION_CHANGE reply internally.
---------------------------------------------------------------------------

## 59. in `push_slot_to_device()`

No param-names section for the Morph/patch-settings location — a patch file only ever carries VA+FX param-names sections, and patch-settings pseudo-modules
never have param labels. write_param_names() unconditionally emits a section header even when
empty, so calling it for Morph would add a spurious 3rd 0x5b section (see write_patch_to_file()).

## 60. `send_play_note()`

Virtual Keyboard note on/off: command 0x56, then two 8-bit values, the note-on/note-off flag and
the MIDI note number - the same field width as SUB_COMMAND_ASSIGN_MIDICC's location and
param-index bytes. No slot field: and the synth
routes the note by its own keyboard assignment, as it would a note from the real keys.

THE FLAG IS INVERTED FROM THE OBVIOUS READING, AND THIS IS HARDWARE-CONFIRMED (owner, 2026-08-02,
by ear): ZERO SOUNDS THE NOTE, ONE RELEASES IT. Sending 1 for on gave a keyboard that was exactly
backwards — silent on press, sounding on release. So 1 is the note-OFF action. Only the hardware
said which way round it runs.

COMMAND_WRITE_NO_RESP rather than COMMAND_REQ deliberately, and no send_and_receive(). A held
key can fire many of these, and an unconsumed ack left in the pipe is exactly what desynchronises
the next command's receive (see the patch-version note in menus.c). send_set_param_value() is the
precedent: the app's highest-traffic command, no-response, known good. If the G2 turns out to ack
this one regardless, that is the first thing to revisit.

Velocity is accepted by the caller but NOT sent: the command carries two values only. If notes
come out at a fixed velocity on hardware, that is why, and it matches what the original does.

## 61. `send_ctrl_snapshot()`

Send Controller Snapshot — the original's Synth > Send Controller Snapshot (Ctrl-M). Makes the
G2 transmit the current value of every MIDI-CC-assigned parameter, so an external device or a
sequencer's automation lane can be brought into line with the patch in one go.

A BARE COMMAND WITH NO PAYLOAD: 0x55 and nothing else. It has the same shape as the current-notes
request (0x68), which send_get_current_note() already sends as usb_cmd_slot(COMMAND_REQ) with no
payload - and works - so this follows it exactly.

## 62. `apply_synth_settings_restore()`

Applies whatever peek_synth_settings_restore() staged, once the user has confirmed. Copies the
staged struct into the live gSynthSettings and pushes it to the device via send_synth_settings()
just above — the same wire path the Settings panel's own "apply" already uses, so no new wire
protocol was needed for this feature at all.

## 63. `restore_everything()`

Mirrors backup_everything(): walks all Patch Banks, all Performance Banks, then Synth Settings,
restoring each from srcFolder in one sweep. Unlike a single restore_bank() call — where a missing
manifest is almost certainly a wrong-folder mistake, and so refuses to touch the device at all —
a missing manifest here is the normal, expected case (e.g. an incremental backup that only ever
covered some banks), so this just skips that one bank and moves on, leaving it untouched, rather
than treating it as a failure. Synth Settings restore is skipped the same way if no backup file
is found. A genuine device-level failure (disconnected, a push/clear rejected) still stops the
whole sweep immediately and reports exactly how far it got, same rule as restore_bank() itself.

## 64. in `send_perf_mode_change()`

── The bank name sweep, run in the background ──────────────────────────────────────────────────

SUB_COMMAND_LIST_NAMES (0x14), sent repeatedly, following whatever continuation
parse_list_names_response() reports, until both the Patch and Performance domains report
"finished" — reading every currently-populated location's name and category, device-wide.

IT USED TO RUN AS A BLOCKING LOOP AT THE END OF THE PULL, and cost 8,012 ms of the 8,128 ms
before the editor called itself online (the patch data itself is 116 ms of that). So it is a
paced background task now: name_sweep_step() sends exactly ONE request per pass of
state_handler(), and only when the UI has nothing queued, so an interactive edit never waits
behind a bank read. The names are wanted by the Load/Store/Delete pickers and by nothing else,
and nameCache.h remembers them between runs, so in the ordinary case the pickers are already
populated from cache.txt before the sweep confirms them.

It writes into a STAGING pair of tables and swaps them in whole on completion, so the live
tables — and therefore the pickers — never show a half-read bank. A sweep that never completes
is discarded rather than swapped: a partial list is worse than a stale one.

## 65. `name_tables_edited()`

Called after Store, Delete and Bank Restore, which edit the live tables directly from what they
just told the device to do — so the remembered copy has to follow. A sweep in flight holds a
staging copy that predates the edit and would undo it on the swap, so it starts again rather
than finishing with data it read before the change.

## 66. in `name_sweep_step()`

THE DEVICE ONLY EMITS ITS INLINE 0x03 [bank][location] transition when the next bank's
entries are in the SAME response. A response that ends exactly on a bank boundary carries
no transition — it comes back saying "bank b, location 128" — and asking for location 128
of bank b returns that same answer again, forever. Rolling the bank over here is what
makes the sweep terminate. It is also where the old blocking sweep's eight seconds
actually went: it spun on bank 1 until its 2000-request guard tripped, gave up on the
whole domain, and did the same again for performances. Every bank past the first was
never read at all, which is why the pickers only ever showed Bank 1.

## 67. in `send_init_sequence_pull()`

THE NAME SWEEP IS NO LONGER PART OF THIS SEQUENCE. It was 8,012 ms of the 8,128 ms this
function used to take, for data no part of the editor needs in order to open a patch. What
happens instead: last run's names come back off disk immediately, and the sweep that confirms
them is armed here and paced out one request at a time by state_handler().

The tables are NOT cleared first. The cache is the best answer available until the sweep
replaces it wholesale, and clearing would leave the pickers empty for the eight seconds it
takes to fill them again — which is the entire delay this change exists to remove. Stale
entries are dealt with by the swap at the end of the sweep, not by a memset at the start.

## 68. in `send_init_sequence_pull()`

Push all editor state to the G2. Not called on reconnection; reserved for
a future "push to device" menu action.

MARKED UNUSED ON PURPOSE. It is finished code waiting for a caller, so it should not be deleted to
quieten -Wunused-function, and it should not leave a warning standing either — a real one would be
lost among a set of accepted ones.

## 69. `read_g2_file_payload()`

Loads a .prf2 performance file straight into the device, entirely on the USB thread. This runs as
part of one command (eMsgCmdLoadFile) so the whole sequence — switch to perf mode, clear the database,
parse the file into it, then push it to the device — happens without interruption. That matters
because switching to perf mode makes the G2 raise patch-change indications for the OLD perf;
state_handler() only services those (re-reading device patches into the database) BETWEEN queued
commands, never mid-handler, so doing the parse here rather than on the UI thread means the
cascade can't overwrite the freshly-parsed perf before it's written. Replaces the old UI-thread
path that used a blind 2-second sleep as a crude barrier against exactly that race (see
read_file_into_memory_and_process(), graphics.cpp). Returns EXIT_SUCCESS only if the file parsed
and the device write succeeded.
Reads a .pch2/.prf2 into a freshly malloc'd buffer (*outBuf, caller frees), validates its CRC, and
returns the PAYLOAD offset/length — past the ASCII header line (to the first 0x00) plus the
version + type bytes. Framing matches read_file_into_memory_and_process() (graphics.cpp); shared by
the perf- and patch-load handlers so it lives in one place.

## 70. in `load_perf_from_payload()`

The mode switch (and its ensuing device activity) may have raised patch-change / perf-settings
indications for the perf we're replacing. Drop them so a later state_handler() pass doesn't
read the old device state back over what we're about to parse and write. (Note we do NOT
suppress gotPerfSettingsChangeIndication the same way — after the write we deliberately
re-raise it, below, to drive the view refresh.)

## 71. in `load_perf_from_payload()`

Drive the standard "performance changed" view refresh (state_handler's
gotPerfSettingsChangeIndication branch: reset to slot A / variation 1, re-read perf settings,
full redraw). Without this the freshly-loaded current slot's patch name and module canvas
don't repaint until the user manually re-selects a slot. That branch only re-reads perf
SETTINGS (names etc.), not module data, so it can't clobber the perf we just parsed and wrote.

## 72. `load_patch_from_payload()`

Loads a .pch2 patch file into one edit-buffer slot, entirely on the USB thread. Single-slot
analogue of load_perf_from_payload: doing clear+parse+push in one ordered command keeps the parse
from being clobbered by the USB thread's own async patch-change re-reads (which state_handler only
services BETWEEN commands). Mirrors read_file_into_memory_and_process()'s old patch branch, moved
off the UI thread. Takes the already-read payload; frees it.

## 73. `command_needs_stop_start()`

Whether a send_write_data() case needs the unsolicited-message stream paused (send_stop()/
send_start()) around its own device writes. True for every command except: eMsgCmdSetValue/
eMsgCmdSetParamMorph (real-time param/morph tweaks, left unpaused so they stay responsive
while dragging) and eMsgCmdSetCustomData/eMsgCmdPeekSynthSettingsRestore (no device write at
all). An unrecognised cmd value falls through to send_write_data()'s own default: case (a
no-op log), which harmlessly gets a pointless stop/start bracket rather than none — that
combination should never actually happen.

## 74. in `send_write_data()`

the rest of parse_performance_settings()'s data) is shared
with patch mode, not perf-only — see parse_performance_settings()'s
own comment on gGlobalSettings.masterClock. Without this,
switching to patch mode left the clock display showing
whatever was last fetched until the app was restarted.
