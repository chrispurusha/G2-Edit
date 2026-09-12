# moduleResourcesAccess.c notes

The longer comments from `moduleResourcesAccess.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `module_device_param_count()`

How many parameters the DEVICE sends for this module type, which is not always how many rows the
table has. paramTypeCustomData rows are local storage, not wire parameters: SeqNote's Magnifier
and Octave sit in param slots past the end of what the G2 transmits, and reach the device through
their own eMsgCmdSetCustomData message instead (see send_custom_data_value() in protocol.c).

Only the patch-parse count check wants this. Everywhere else — init, copy/paste, the sound engine —
genuinely means "every slot this module uses", which is module_param_count() above.

Called from both threads — caches are pre-warmed by init_module_resource_cache() before USB thread starts.

## 2. `populate_module_connectors()`

A connector's direction and type are STATIC per module type: they restate what
connectorLocationList already holds, which is why tConnector's own fields carry the note
"Should pull from the location list".

They used to be written only as a side effect of DRAWING, in render_connector_common(). The
application always draws, so the array was always filled and nothing looked wrong — but
anything that renders no pixels (the VST3 plug-in, any headless harness) saw a zeroed array,
and every cable lookup that reads .dir then failed silently. The sound engine's modulation
inputs simply never connected, which presents as a filter that plays shut rather than as an
error. Populating at load time makes the data available whether or not anything is drawn.

Only .dir and .type are set here. .coord and .rectangle are genuine geometry — they depend on
where the module is actually rendered — and remain the renderer's to fill in.

## 3. in `module_mode_count()`

Every caller uses this count to walk a MAX_NUM_MODES-sized array — module->mode[], the
message's mode[], the click-context table's last dimension. It used to be impossible for
the table to outrun them (16 slots against a table whose fullest type has 2), so none of
those loops check. Clamping here keeps that true from one place, whatever gets added to
modeLocationList later.

## 4. `module_led_row_count()`

Called from both threads — caches are pre-warmed by init_module_resource_cache() before USB thread starts.
How many LED rows the module HAS, of every kind. This is a DRAWING question — how many boxes go on
the face — and it is deliberately not the same as module_led_count(), which answers a wire
question: how many 2-bit values the module takes out of the 0x39 stream. They were the same thing
until multi-bit groups were separated out, and the renderer bounding its loop with the wire count
drew one LED where an 8Counter has eight.

## 5. in `module_led_count()`

ONLY ledTypeYes. This is the count of 2-bit values the module takes out of the 0x39 LED
stream, and that is one per SINGLE-LED GROUP — not one per LED. A module whose LEDs form a
multi-bit group (8Counter and friends) takes ONE value out of the multi stream instead and
none out of this one; counting its eight here consumed eight slots that belong to the
modules after it, which is why LEDs were right in most patches and wrong in any patch
containing one of those. ledTypePark takes none either.

## 6. `default_mutation_lock()`

Patch Mutator "Exclude From Mutation" default per module type, the instrument's own defaults, confirmed against 395 real captured patches (see
mutator.c). Applied to newly created modules. (Previously also reapplied on every patch reparse
for old-format patches - removed 2026-07-15 once live writes via SUB_COMMAND_SET_MUTATION_LOCK
were confirmed working on hardware, since that reapply was clobbering live-toggled bits on
every resync. Old patches now simply trust whatever's on the wire, same as new ones.)
