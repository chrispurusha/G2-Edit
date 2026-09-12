# backdoor.c notes

The longer comments from `backdoor.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `backdoor_enabled()`

── Backdoor test-control channel ───────────────────────────────────────────
A way to drive AND independently verify the running app — load a patch,
select a slot, dump the module list, or capture a screenshot — without a
real mouse click or a reliable headless paint event. Ported from SynthEdit's
proven mechanism (SynthEdit/src/graphics.cpp), adapted to G2's domain.

GATED behind the G2_EDIT_BACKDOOR environment variable: unset (the owner's
normal double-click launch) => backdoor completely inert AND the idle loop
keeps glfwWaitEvents()'s full sleep. Set (a test launch from a shell) =>
the idle loop polls at 10 Hz and a command file is honoured each tick. Unlike
SynthEdit (sandboxed, needs its container tmp dir) G2-Edit has no App Sandbox,
so plain /tmp works. Command surface is deliberately narrow and does nothing a
real mouse click couldn't already do.

Command file (/tmp/g2edit_cmd.txt): one command per file, first line only,
"<COMMAND> <arg>". Result ("OK\n"/"ERROR: ...\n", or DUMP's own text) is
written to /tmp/g2edit_result.txt and the command file is deleted, so a
caller polls for the command file's disappearance to know it's done.
```
  LOADFILE <path>   — read_file_into_memory_and_process() (works offline)
  SLOT <0-3|A-D>    — select the slot the canvas renders
  COMMS             — "online" or "offline". ASK THIS BEFORE ANY DEV COMMAND YOU INTEND TO TRUST:
                      every one of them reports OK when the instrument is not listening
  DUMP              — current slot + every module: type, name, location, col/row
  LEDDUMP           — live LED and volume-meter values per module, as the renderer reads them.
                      Poll it to measure a blink RATE, which no screenshot can show
  PARAMDUMP         — the same modules with their variation-0 PARAMETER and MODE values, plus the
                      parameter count the patch declared against the one our table gives
  MENU <bar>[/<item>[/<sub>]] — run a menu item by label (leading substring, case-insensitive,
                      '/' separated); omit the last level to LIST what that level contains
  SELECT <VA|FX> <n> — select one module by index; SELECT NONE clears
  REPLACE [VA|FX] <index> <name>|LIST — swap a module for another of the same group, as the
                      right-click menu does. Local only; check the result with DUMP
  PALETTE ON|OFF|TOGGLE|STATUS | GROUP <name> | ADD <module> — drive the module palette. The
                      band changes the topbar height and so the canvas origin, so TOGGLE is here
                      to be hammered; ADD is the double-click path, since a synthetic drag never
                      reaches the canvas
                      right-click menu does. Local only; check the result with DUMP
  SNDSTATUS         — what the sound engine's status line currently reads
  SNDDUMP           — the resolved chain, the parameters read, and the peak level since last read
  NOTE <n>|OFF      — play/release a note on the sound engine (LOCAL engine, not the G2)
  DEVSET <VA|FX> <index> <param> <value> — as SET, but SENT TO THE G2. This is what lets the
                      measurement harness step one parameter on the hardware while its audio output
                      is recorded; SET stays local-only so a rendering test cannot write to a
                      connected synth by accident.
  DEVMODE <VA|FX> <index> <mode> <value> — a MODE write to the G2 (the drop-down selectors: the
                      Reverb's room size, a filter's slope, an oscillator's waveform). Modes travel
                      on their own wire command, so DEVSET cannot reach them.
  DEVKNOB <knob 0-119> <VA|FX> <index> <param> — assign a parameter to a patch knob, on the G2 as
                      well as locally. Needed before the synth's own display can be asked what a
                      dial reads: an unassigned parameter has nowhere to show itself on the panel.
  DEVNOTE <note> <vel> on|off — a Virtual Keyboard note to the G2, for a patch that needs a gate
                      rather than a free-running clock (envelope times, for instance). NOT A
                      PLAYING PATH: it will not start a note while one is releasing, identically
                      at one voice mono and eight voices poly. Use tools/g2_note over MIDI when
                      the NOTE BEHAVIOUR itself is what is being measured
  DEVNOTES [slot]   — which notes the INSTRUMENT believes are held, decoded from its reply as a
                      7-bit note, attack and release. Waits for a FRESH reply rather than
                      reporting a stale buffer, so a disconnected G2 cannot look like a working
                      query. This is how a note fault is observed instead of inferred
  VOICES <1-32> [poly|mono|legato] — the patch's voice allocation, locally; PUSH sends it. A
                      gated measurement cannot be read without knowing this: an envelope that
                      seemed not to retrigger was a one-voice mono patch whose voice was busy
  DEVADDMODULE [VA|FX] <name> [col] [row]  — as ADDMODULE, but created ON THE G2 too. The area
                      is optional and defaults to VA; FX is the only scripted route into the
                      effects area, and without it no test patch can have LEDs in both.
                      OMIT THE ROW to pack it directly under whatever is already in that
                      column, from the real module heights rather than a guessed gap
  DEVDELMODULE <VA|FX> <index>     — deletes a module and its cables, on the G2 as well as here
                      These two exist because every LOCAL-ONLY command can only ever produce a
                      freshly-loaded patch, and that is the one state where the LED stream is known
                      to behave. An edit the DEVICE sees is what is needed to chase the LED
                      ordering fault, and without these it could only be done by hand in the GUI.
  SELECTADD <VA|FX> <index> — add to the selection rather than replace it
  MOVESEL <dColumn> <dRow>  — move the whole selection by a grid delta and re-order the column as a
                      drop does. The only scripted route to a GROUP drop, synthetic drags being
                      invisible to the app; refuses with "no room" and puts the group back
  SAVEFILE <path>   — write the current slot to a path (no save panel)
  SCREENSHOT <path> — synchronous render_frame() then glReadPixels + PNG
  SCROLL <x> <y>    — scroll the canvas, each 0.0-1.0 of that axis's full travel
  ZOOM <factor>     — canvas zoom, same 0.25-2.0 range Cmd +/- walks through
  SPLIT <VA|FX|BALANCE|pixels> — where the Voice/FX divider sits. VA gives the Voice Area the
                      whole window (the FX area minimised), FX the reverse, BALANCE the
                      double-arrow's restore; a number is a Voice Area height in pixels. Framing
                      for a render check: with the divider halfway, a tall module does not fit

```
SCROLL and ZOOM exist because a synthetic drag doesn't reach the app at all — neither the
scrollbar thumb nor a dial responds to one — so without them a scripted check can only ever see
the modules that happen to be on screen at the default zoom.

## 2. in `backdoor_dump_state()`

VOICE COUNT AND MONO/POLY BELONG IN THE DUMP. How a patch allocates voices decides whether a
fast second note sounds at all, so a measurement that gates notes cannot be read without it —
an observation that looked like an envelope-retrigger difference turned out to need this to
be interpreted. monoPoly is 0 Poly, 1 Mono, 2 Legato.

## 3. `backdoor_led_dump()`

LEDDUMP — the live LED and volume-meter state of every module in the current slot, exactly as
render_module() reads it. Eyeballing a screenshot cannot answer "which module is stream index 3",
and a blink RATE is invisible in a still; this reports the numbers the renderer draws from, so a
caller can poll it and count transitions per module.

leds= is one 0-3 value per LED in ledLocationList order (the order parse_led_data() fills), vols=
one 0-255 per meter. A module with neither is skipped, which keeps the output to the few modules
an LED test actually cares about.

## 4. `backdoor_send_cable()`

PARAMDUMP — every active module in the current slot with its VARIATION 0 parameter values and its
mode values. DUMP above reports only STRUCTURE (which modules, which cables); this reports
CONTENTS, which is what auditing paramLocationList's defaultValue column against a reference patch
needs.

Prints the count the PATCH declared (module->actualParamCount) alongside the count our own table
gives, so the same dump doubles as a param-count check on any device-authored file.

Written straight to the result file rather than composed in a buffer the way backdoor_dump_state()
is: a full patch runs to tens of modules by tens of parameters, which overruns any fixed buffer
worth putting on the stack.
Tells the instrument about a cable the backdoor just added or removed, exactly as the drag-connect
and Disconnect paths do. Nothing happens when offline, which is what makes the same script usable
as an offline layout scratchpad.

## 5. `backdoor_connector_for_io_index()`

A cable end is addressed by its I/O index — "output 2", "input 0" — counting only connectors of
that direction. That is how the protocol expresses it, how tCableKey stores it, and how the DUMP
command above prints it. A module's connector array is in declaration order with both directions
interleaved, so turning one into the other is a walk. -1 if the module has no such connector.

## 6. `backdoor_next_free_row()`

The next free row in a column, worked out from the heights gModuleProperties already carries.
A scripted patch used to have to guess its own spacing, and a guess is either loose - the envelope
measurement patch sat at rows 0/6/11/18 where those modules are 4/2/5/2 tall, so it wasted eight
rows and needed scrolling to see - or too tight, which lands one module inside another. Placement
was only ever a guess because the caller cannot see the heights; here they are.

## 7. `backdoor_connector_is_input_end()`

An input connector takes at most one incoming cable — the same invariant cable-drag creation
enforces before it commits. Checked here too, because the device silently keeps whichever it
likes when told otherwise.
Is this connector already serving as the input end of some cable? An input takes exactly one
cable, so this is what stops a scripted CABLE from stacking a second one on top of an existing
connection - which looks like nothing at all on screen, because the two are drawn along the same
path, and only shows itself when you pull one off and the sound stays.

BOTH ENDS OF A cableLinkTypeFromInput CABLE ARE INPUTS (see cableChain.h): that link type is the
G2's input-to-input daisy chain, so its from-end is an input just as much as its to-end is. This
used to test the to-end alone, so an input already spoken for as the FROM-end of such a chain
read as free and got a second cable.

## 8. in `backdoor_dispatch()`

Clears the current slot's canvas — AND the instrument's, when there is one. It used to
clear only the local database, which quietly left the G2 holding the previous patch: a test
that built a "clean" patch on the device inherited the old one's cables, and the LEDs it
then reported were correct for a patch nobody could see. Divergence between our copy and
the edit buffer is the one thing a test harness must not introduce.

## 9. in `backdoor_dispatch()`

PALETTE ON|OFF|TOGGLE|STATUS | GROUP <name> | ADD <module>
The band changes the topbar's height and so the canvas origin, which is the part of this
feature most likely to break something else; TOGGLE exists so that can be hammered from a
script. ADD is the double-click path - a synthetic DRAG never reaches the canvas at all.

## 10. in `backdoor_dispatch()`

REPLACE [VA|FX] <index> <name> - swap a module for another of the same group, the way the
module right-click menu's "Replace with" does. Local only: module_replace() posts one
whole-patch write, which goes nowhere when there is no instrument attached, so this drives
exactly the code path the menu drives and can be checked with DUMP afterwards.

## 11. in `backdoor_dispatch()`

BOTH SPELLINGS REACH THE INSTRUMENT. ADDMODULE was local-only, which meant a scripted patch
and the G2's edit buffer could drift apart without anything saying so — and every LED, knob
and cable index the device reports is relative to ITS copy. DEVADDMODULE remains as a
synonym so existing scripts keep working.

## 12. in `backdoor_dispatch()`

ADDMODULE [VA|FX] <name> [col] [row] — name matches gModuleProperties[].name
(e.g. "Mix4-1C"). The area is optional and defaults to VA, which is what this command did
before it existed, so every existing script is unaffected.

THE AREA ARGUMENT IS NOT A CONVENIENCE. Until it was added there was no scripted way to
put a module in the FX area at all, and that shows in the test corpus: all 18 files in
PatchTestFiles have their LED-bearing modules in VA and none in FX. The 0x39 stream's
index space is the two areas concatenated, so with one of them empty both possible area
orderings give the same answer and every LED test we have passes either way. Settling
which order the instrument really uses needs LEDs in both areas — see todo.md.

## 13. in `backdoor_dispatch()`

OMIT THE ROW AND THEY PACK. With a row given, create_module_at() still ends in
shift_modules_down(), so an explicit row landing on top of something pushes it out of the
way rather than drawing garbled - but only an omitted row is placed tight against whatever
is already in the column.

## 14. in `backdoor_dispatch()`

DEVADDMODULE syncs to the instrument where ADDMODULE stays local — the DEV prefix means the
same thing here as it does on DEVSET and DEVMODE. It exists because the LOCAL-ONLY commands
can only ever produce a freshly-loaded patch, and a freshly-loaded patch is exactly the
state where the LED stream is known to behave: reproducing the ordering fault needs edits
the DEVICE sees, which until now meant driving the GUI by hand.

## 15. in `backdoor_dispatch()`

CABLE / DELCABLE <VA|FX> <from>:<out> <to>:<in> [link=<0|1>]

SENT TO THE INSTRUMENT, one message per edit — the same eMsgCmdWriteCable/eMsgCmdDeleteCable
the drag-connect path sends, so a scripted cable is indistinguishable from a drawn one.

These used to be local-only, with PUSH afterwards to send the slot in one versioned
command. That is still the right shape for a BULK run (see the note above
send_whole_patch() in menus.c: a burst of per-entry commands can race the G2's asynchronous
version notification), and PUSH is still there for it. But local-only as the DEFAULT let a
script and the edit buffer drift apart silently, and every index the device reports back —
LED slots above all — is relative to ITS copy, not ours. A harness that can lie about what
the instrument holds is worse than one that is occasionally slow.

## 16. in `backdoor_dispatch()`

THE COLOUR ABOVE ONLY GUESSES, and two separate things correct it — the drag path does both,
and until now this did only the second.

FIRST, RE-DERIVE THE CHAIN'S COLOUR ACROSS THE WHOLE TREE, which is what maintains the
invariant the guess cannot: every cable in a chain carries ONE colour, its source output's,
or WHITE when the chain has no source at all. A scripted fan-out inherited each cable's
colour from its own from-connector and so could paint one chain two colours, and a scripted
input-to-input link — which is sourceless and must come out WHITE (the manual's
"non-functional input-to-input connections") — came out whatever the from-input happened to
be. Neither showed up in the measurement patches built so far, because every cable in them
takes its colour from its own source, which is exactly the sort of luck that stops being
true the first time a patch fans out.

The to-end is always an input, whichever link type this is, so the node needs no database
lookup — the same construction canvasDrag.c uses. cable_chain_apply_colour() sends
eMsgCmdSetCableColour per cable it actually changes, so the instrument follows; it is a
RECOLOUR and not a write, which is what stops the G2 holding each cable twice.

## 17. in `backdoor_dispatch()`

SECOND, re-assess up-rate across the slot. Feeding an audio output into a multi-bandwidth
(Control/Logic) input promotes the DESTINATION module to audio rate, which repaints the
cables leaving it and changes the rate the G2 runs it at. Before 2026-08-24 a scripted patch
never did this, so backdoor-built patches drew a promoted module's outputs in the wrong
colour AND left the instrument running it at control rate, with eMsgCmdSetModuleUpRate never
sent. Ordered after the recolour, as in canvasDrag.c.

## 18. in `backdoor_dispatch()`

DEVSET <VA|FX> <index> <param> <value> — like SET, but SENDS THE CHANGE TO THE G2 as a dial
drag would, instead of only touching the local copy.

This exists for the measurement harness: characterising a module means stepping one of its
parameters over its range while recording the device's audio output, and that only works if
the hardware actually follows. SET stays local-only for its own purpose (seeing how a value
RENDERS), and the two are deliberately separate commands so a rendering test can never
write to a connected synth by accident.

Nothing here is destructive: this is a live parameter edit, exactly what the canvas does on
every drag, and it does not store to flash.

## 19. in `backdoor_dispatch()`

VALIDATED AGAINST THE MODULE'S OWN PARAM COUNT, not against the array bound. MAX_NUM_PARAMETERS
is the size of the store, not the number this module has, and a write past the real count goes
out on the wire, gets dropped by the G2, and reports OK — the local copy has already changed,
so nothing anywhere says the device disagreed.

THAT COST THREE MEASUREMENT RUNS. The Reverb's Small/Medium/Large/Hall selector is a MODE, and
a sweep that drove it as `DEVSET <index> 4` produced three files of the same room with no
complaint from anything. The lengths only came out identical because they genuinely were.

## 20. in `backdoor_dispatch()`

DEVKNOB <knob 0-119> <VA|FX> <index> <param> — assign a parameter to one of the patch's
knobs, on the G2 as well as locally. The same thing the canvas's Assign Knob menu does.

This is what makes a hardware reading possible at all: a parameter the panel has not been
pointed at cannot be shown on the synth's display, so a question of the form "what does
this dial actually read" needs the assignment before it needs the value.

Knob numbering is the 0-119 the patch stores: 24 to a page, 8 to a bank within it, so
knob 0 is page 1 bank A position 1 — the first knob of the first page.

## 21. in `backdoor_dispatch()`

DEVMODE <VA|FX> <index> <mode> <value> — a MODE write to the G2, the companion to DEVSET.

Modes are the drop-down selectors, and they travel on their own wire command rather than as
parameters: the Reverb's Small/Medium/Large/Hall is a mode, as are a filter's slope and an
oscillator's waveform. Measuring across those settings is exactly what the harness is for, so
without this the most valuable sweep of all — the four reverb room sizes, which are data that
exists nowhere else — could not be driven at all.

## 22. in `backdoor_dispatch()`

VOICES <1-32> [poly|mono|legato] — set the patch's voice allocation, locally. PUSH sends it.

Exists because a gated measurement cannot be interpreted without it. An envelope that
appeared not to retrigger during its release turned out to be a ONE-VOICE MONO patch whose
single voice was still busy — allocation, not envelope behaviour. Being able to set this
is what separates the two.

## 23. in `backdoor_dispatch()`

DEVNOTES [slot] — ASK THE INSTRUMENT WHICH NOTES IT THINKS ARE HELD.

Exists because DEVNOTE's failure could not be diagnosed by inference. Every send succeeds
and the G2's envelope LED lights, yet most gates produce no sound — so the question "does
the instrument still believe an earlier note is down?" had no way of being asked. This
asks it, which turns that fault from a score out of eight into an observation.

WAITS FOR A FRESH REPLY, never reporting whatever is already in the buffer: gNote2Updates
is bumped by store_note2() when one lands, so this samples it first and waits for it to
move. Without that a disconnected instrument would return the last good answer and look
like a working query.

## 24. in `backdoor_dispatch()`

ONLINE OR OFFLINE, ASKED DIRECTLY — and it exists because nothing else here can tell you.
Every DEV command reports OK whether or not the instrument is listening: they update the
local database first and post to the USB thread second, and that post is a no-op when
nothing is connected. So a measurement sweep driven at an offline editor runs to
completion, writes its files, and records SILENCE, with every step along the way saying
OK. That happened on 2026-08-24 and cost a capture that looked perfectly valid.

The obvious tell is missing too: the top bar says "Offline" both when no G2 is plugged in
and when another copy of the editor holds the USB claim.

## 25. in `backdoor_dispatch()`

MENU <bar>[/<item>[/<subitem>]] — runs a menu item by label without going near the mouse.
Labels are matched case-insensitively on a leading substring and separated by '/', so
'MENU Exp/Audio Device/QU-24' reaches into a flyout. Any trailing level omitted, the
deepest menu reached is LISTED instead of clicked, which is how a test discovers what is
there. Driving these by screen coordinates meant re-deriving them whenever a window moved.

## 26. in `backdoor_dispatch()`

MOVESEL <dColumn> <dRow> — moves the whole selection by a grid delta and then re-orders the
column exactly as dropping it would, through the same shift_selection_down() the release
calls. THE ONLY SCRIPTED ROUTE TO A GROUP DROP: a synthetic drag does not reach the app, so
without this the multi-module half of the shift can only be exercised by hand.

Refuses with "no room" when the shift cannot place the group, and puts it back where it
was — the same answer canvas_module_drag_release() gives a drag it cannot land.

## 27. in `backdoor_dispatch()`

SPLIT VA | FX | BALANCE | <pixels>

Where the Voice/FX divider sits. VA and FX slam it to an end, which is exactly what the
topbar's VA/FX buttons and the bar's own up/down arrows do; BALANCE is the double-arrow.
A number is the drag, as a Voice Area height in pixels, clamped the same way.

This exists for RENDER CHECKS. A screenshot of a module face is framed by whatever the
divider leaves, and with the FX area taking half the window a four-row module does not
fit — so checking a face meant scrolling around it a screenshot at a time (CT: "You may
want to minimise the FX area via the dividing line when attempting to check the
renders"). A synthetic drag does not reach the app, which is the same reason SCROLL and
ZOOM are here.

IT IS PATCH DATA, NOT A VIEW SETTING, and that matters for a measurement run. The divider
lives in gPatchDescr[slot].barPosition, so moving it marks the patch dirty and it travels
to the G2 and to file like any other edit - unlike SCROLL and ZOOM, which are purely
local. Frame with SPLIT before building the patch under test, not in the middle of one.
