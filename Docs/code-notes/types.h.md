# types.h notes

The longer comments from `types.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

AFTER defs.h, and it has to stay that way. This reaches synthlibTypes.h -> synthlibDefs.h,
where every RGB_* colour sits behind #ifdef G2_EDIT — and G2_EDIT is defined in defs.h.
Include it first and synthlibDefs.h is consumed with that guard closed; its own include
guard then stops the later #include from doing anything, and the build fails much later in
unrelated files with "undeclared identifier RGB_GREY_9". The ordering is load-bearing.

## 2. `tModuleType`

MODULE TYPES, INDEXED BY THE NUMBER THE PATCH FORMAT USES — so the order is the instrument's, not
ours, and the moduleTypeUnknownNN entries are real slots we have not identified rather than padding
to be tidied away. gModuleProperties[] in moduleResources.h is this list's name table and must stay
in step with it.

NAMES THAT DIFFER FROM WHAT THE G2 CALLS THEM — check here before concluding a module is missing:
```
    moduleTypeSandH        the instrument labels it "S&H" ('&' is awkward in an identifier)
```
That is the only alias at present. Everything else either matches the instrument's own label or is
still an Unknown slot.

STILL UNIDENTIFIED, and known to exist: comparing this table against the full set of modules the
instrument supports leaves 22 of its names with no slot filled here. In rough order of how likely
they are to matter for the editor's graphics work:
```
    ShelvEQ  PulseOsc  LfoD  EnvDX  AR-Env  SeqA  Mux8-1X        (all draw a graph on the face)
    SyncOsc  S&H  T&H  PeakFollow  RndStep  RndState  RndChaos
    PolarPan  PolarFade  Mixer6-1A  Mixer6-1B  OutBusA  OutBusB  ClkDivFix  AudioIn  BusIn
```
HOW TO FILL ONE: the type NUMBER is what is missing, and the instrument's own patches carry it —
load a factory patch that uses the module and DUMP reports its type against a name we render as
"Unknown". That is the only route; nothing about the number is derivable from the name.

## 3. file scope

Channel Select radio buttons — the Switch modules' selector. One button per channel, laid out
in a grid, the selected one lit, and EACH BUTTON separately renamable (manual p221: "These
Channel Select buttons can also be labelled... Right-click on a Channel Select button and
select 'Edit name'"). The names live in paramName[param][label], which the protocol has always
carried a count of — nothing but this type has ever used more than label 0.

## 4. file scope

The Switch modules' Ctrl output value. Manual p221 ("Common Switch parameters"): the Control
output "sends out the Ctrl signal value 0 for the initial state, value 4 for the next state,
value 8 for the next and so on", to a maximum of 28 on an eight-way switch — which is what the
Mux modules decode (0<4 = channel 1, 4<8 = channel 2 ...), so that a Switch button always
reaches the same Mux channel whatever the button count.

## 5. `tLed`

One entry per LED the module has, in ledLocationList order. ledRef and rectangle used to sit here
too; both were write-only, and once value became an array a single ledRef could only ever hold
whichever LED was drawn last — a field that looks like it identifies the LED but does not. The
renderer already has the ref it was passed. (tVolume above still carries the same write-only
volumeRef.)

## 6. file scope

SUB-UNIT REMAINDER CARRIED BETWEEN MOUSE-MOVE EVENTS, and the reason slow dragging used to do
nothing at all. An incremental drag converts the movement since the PREVIOUS event into whole
parameter units and then advances its reference point — so a movement worth less than one unit
truncated to zero and was thrown away, every event, no matter how far the pointer travelled in
total. Moving slowly therefore changed nothing, and a fine (Shift) drag changed nothing at all,
because dividing by ten times as many pixels makes almost every event sub-unit.

Keeping the fraction here and adding it to the next event's makes a slow drag advance smoothly
instead of not at all. It lives in this struct rather than as a file static so it is zeroed for
free by the memset that arms every drag — a leftover fraction from the last drag would otherwise
be spent on the first event of the next one.

## 7. file scope

THE RECT THE DRAG WAS ARMED ON, captured at press time rather than looked up per mouse-move.

Rotary mode needs the widget's centre to turn a cursor position into an angle, and it used to
fetch that by indexing gParamRectangle every event. That is a re-derivation of something the
press already knew, and it is what standard pointer capture exists to avoid: the owner of a
gesture holds what it needs for the duration, so the gesture cannot be disturbed by anything
that re-registers, re-renders or re-orders underneath it mid-drag. It is also one of the two
readers that kept the 6MB gParamRectangle array readable at all.

## 8. file scope

RE-ROUTING an existing cable rather than drawing a new one. Ctrl-click on a connector picks up
the cable already there and drags its free end: drop it on another connector and the cable
moves, drop it on nothing and the cable is gone. Manual p65: "double-click-hold or Ctrl-click
on a connection... and 'pull out' the connector... If you place the 'disconnected' plug on
another connection instead, the cable will be rerouted."

Recorded at PRESS and acted on at RELEASE, so the whole thing — the delete and whatever
replaces it — lands inside one undo bracket in handle_cable_connect().

## 9. file scope

tMenuItem/tMenuFrame/tContextMenu now live in SynthLib's synthlibTypes.h
(pulled in transitively via geometry.h below) — the mechanism itself is
generic over any app. See tMenuContext further down for the G2-Edit-only
state (moduleKey/paramIndex/etc) that this app's action callbacks need.

## 10. `tModuleGroup`

The module GROUPS the instrument itself uses. Chapter 13 of the manual, "Module reference",
is organised by them - SHAPER GROUP, FILTER GROUP, LEVEL GROUP and so on - and between them
they name every module the G2 has. A group is what makes one module replaceable by another
(manual p.82) and what orders an add-module menu; gModuleProperties carries each module's.

ELEVEN MODULES ARE IN NO GROUP and take moduleGroupNone: the four that never appear on the
canvas (Device, Driver, Name, Status) and seven that are genuinely one of a kind - Blue2Red,
Red2Blue, DXRouter, Resonator, NoteDet, NoteZone and LevScaler. There is nothing to replace
them WITH. LevScaler is the one to know about: the manual documents it inside the Note
chapter, but the instrument does not offer it as a replacement for anything there, so
chapter membership alone is not the same thing as group membership.

## 11. `tPaletteGroup`

A ROLE is what one module's connector or knob MEANS, named so that the same meaning can be found
on a different module in the same group. It is what makes replacing a module intelligent rather
than mechanical: a cable on an FltClassic's pitch modulation input moves to an FltPhase's pitch
modulation input because both fill the group's "Pitch Mod" role - NOT because both happen to be
input 1, which they are not. A role with no counterpart on the new module has nowhere to go, and
that is exactly the case the manual covers when it promises the cables are kept "(if possible)".

`index` counts within its OWN direction - the third input is 2 whatever outputs the module has -
which is the same numbering tCableKey's connectorFromIoCount/connectorToIoCount use, so a cable
end can be remapped without converting anything.
The PALETTE groups - the sixteen the instrument's own Toolbar uses, which are NOT the nineteen
replacement groups above. The manual is explicit that they differ ("the replacement module
pop-ups doesn't always feature exactly the same modules as the module groups in the Toolbar"),
and the difference is not cosmetic: the nineteen leave eleven modules in no group at all, which
is right for replacement (there is nothing to swap a Blue2Red with) and wrong for a palette,
where every module has to be reachable. These sixteen cover all 170.

A module may appear in MORE THAN ONE palette group - NoteDet is in both In/Out and MIDI - so this
is a list of (group, module) pairs rather than a field on the module. gPaletteList is the single
source for both the drag-on palette and the right-click "Create module" menu; there must never be
a second copy of it.

## 12. file scope

The label the CREATE-MODULE MENU shows, which is not the module's own short name: the menu has
room for "Monophonic Keyboard" where a palette tile has room for "MonoKey". Carried per ENTRY
rather than per module because a module in two groups may be named differently in each -
NoteDet is "Note Detector" under In/Out and plain "NoteDet" under MIDI.

## 13. `tLabelLocation`

Static text on a module face — a section heading, not a control. The original editor ships these
as pre-rendered <#Bitmap> images (the Operator's two decode to "Envelope" and "KB Lev Scale");
we draw them as ordinary text in our own style, per Docs/module-layout-rules.md.

DELIBERATELY ITS OWN TABLE, NOT A paramLocationList ROW. A row's POSITION in that list IS the
parameter index — module_param_count() counts rows — so a heading dropped into the middle of a
face would shift every parameter after it and take the wire mapping, copy/paste, undo and the
sound engine with it. paramTypeCustomData gets away with living there only because those rows are
appended past the end, which a heading in the middle of a face cannot be. Same shape, and the
same reasoning, as ledLocationList and volumeLocationList.

## 14. `tMenuContext`

G2-Edit's own record of what the currently open context menu was raised
against — deliberately kept out of the generic tContextMenu (see menus.c),
which knows nothing about modules/connectors/params, only about menu items
and screen positions. Set by whichever open_*_context_menu() raised the
menu; read back by that same menu's action(index) callbacks.

## 15. `tNameTableEntry`

One bank/location's worth of what SUB_COMMAND_LIST_NAMES (0x14) reports — see
gPatchNameTable/gPerfNameTable in globalVars.h. populated distinguishes a real (possibly
zero-length-named) entry from a location the device never mentioned at all (List Names is a
sparse listing — unpopulated locations are never sent on the wire, not represented by a
placeholder entry).
