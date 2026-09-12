# selection.c notes

The longer comments from `selection.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `selection_validate()`

A SELECTION IS ONLY EVER VALID FOR WHAT IS ON SCREEN. Called once per frame from the render loop,
this drops it whenever the ground it stands on has moved: a different slot, a different location,
or the same slot with different modules in it.

WHY A WATCHER RATHER THAN CLEARING AT EACH SITE. There are seven or eight places that can change
any of the three, and they are not all on this thread — a patch arriving because the device's own
patch changed is parsed on the USB thread, and clearing UI state from there would be a data race.
One check on the render thread covers every route, including ones added later.

THE STALE SELECTION WAS NOT MERELY COSMETIC. A module key is slot + location + index, so after a
switch the highlight correctly disappears — is_selected() compares all three — while the keys stay
live. Cut, Copy and Delete are enabled on gSelection.count alone, so they would happily operate on
modules from another slot that were no longer on screen. Worse after a load into the SAME slot,
where those indices now name entirely different modules, so Delete would take the wrong ones.

## 2. in `selection_validate()`

A LOCATION CHANGE NO LONGER DOES, and that was the bug. This test used to read
(slot != lastSlot) || (location != lastLocation), which was correct while the canvas showed
ONE location at a time: switching from the Voice Area to the FX Area put the selected modules
out of sight, and holding a selection you cannot see is the trap this function exists to stop.

Split view shows both at once, so the premise is gone — and the cost was that selecting an FX
module while a Voice Area module was selected took TWO clicks. The first click moved the focus
(split_view_focus_at) AND selected, and this function then threw the new selection away on the
very next frame because gLocation had changed. The second click selected without moving focus,
so it survived. Reported 2026-08-20.

The intent is kept, and stated against what it actually meant: drop the keys that are not on
screen, rather than all of them because a global changed.

## 3. `selection_select_all()`

Select every module in the location currently being viewed. The original's Edit > Select All
(Ctrl-A). Deliberately scoped to ONE location: the canvas only ever shows VA or FX, so selecting
modules you cannot see — and would then Cut or Delete unseen — would be a trap rather than a
convenience.

## 4. in `delete_module_and_cables()`

MIDI Learn's target is the parameter LAST CLICKED, and it deliberately outlives the click — so
deleting the module it names leaves L armed at an index that is now free. Press it and the CC
lands on whatever module is given that index next, which is the same trap the assignment tables
above set, reached from the other end.

## 5. `shift_fit_row()`

The row an incoming module must actually land on for the push below it to fit on the grid, and
whether it fits at all. THE BOTTOM OF THE CANVAS IS A HARD WALL: a module's row runs 0..MAX_ROWS
and no further, so when the block below a drop cannot travel the full dropAmount, the incoming
module comes UP by the shortfall instead of shoving its neighbours off the end.

Before this, both shifts below simply clamped a pushed module to MAX_ROWS. That is not a shift, it
is a pile-up: every module that could not fit landed on the same row, on top of each other and of
whatever was dropped on them. Reported by a user - "on the bottom of canvas, you can place new
modules on top of the existing ones as the editor cannot move the already existing modules
downwards" - and confirmed.

Raising the incoming module is the answer rather than refusing the gesture, because it is what the
two edge clamps in module_drag_motion() already do for the left and top edges: the drop lands as
close to where it was aimed as the grid allows. *fits comes back false only when the column is so
full that even row 0 leaves an overlap, which needs upwards of thirty modules stacked in one
column; the callers refuse the gesture outright in that case, since there is nowhere to put it.

selectionTransparent matches the caller's own walk filter: a multiple selection is transparent to
itself, so its members neither block the drop nor get pushed by it.

## 6. in `shift_fit_row()`

THE BOTTOM OF THE GRID IS A BLOCKER TOO, and it was not treated as one. This function only
ever consulted MAX_ROWS when some OTHER module was in the way: with nothing below it, the
`!hit` return below handed back the requested row untouched, so a tall module placed near the
bottom simply hung off the end of the canvas. Add Module did it as readily as a replace that
grew one - a Vocoder created at row 125 stayed at 125 and occupied rows that do not exist.

Clamping the requested row here covers every caller at once - create, paste, drag, and the
module replace that grows a module in place. The test matches shift_member_place()'s own
`below > MAX_ROWS`, so both places agree on where the grid ends.

## 7. `count_overlapping_pairs()`

Pairs of modules sharing grid squares. Counted rather than merely detected because a patch can
arrive ALREADY overlapping - one saved by a build that still had the MAX_ROWS clamp, or read off a
G2 that was edited by one - and a placement must not be refused for a mess it did not make. Only an
INCREASE over the count taken before the gesture means this gesture broke something.

## 8. `shift_member_place()`

One module placed in its column: cleared below anything it landed inside, then settled against the
room left at the bottom, then the block below it pushed down. Extracted from the two shifts so the
selection can run it per member and take the result back - NOTHING IS SENT HERE. The caller sends
once the placement it is trying has actually been kept, because a trial that gets restored must not
leave the G2 holding rows this side has since abandoned.

shortfall comes back as the distance the module had to be raised to fit. A single module just
keeps that row; a selection must lift EVERY member by the same amount instead, so it throws the
trial away and comes back with the number applied to the whole group.

## 9. in `shift_member_place()`

Dropped INSIDE a taller module, so it clears to just below it — unless just below it is
off the bottom of the grid, in which case it goes ABOVE instead. Clamping `below` to
MAX_ROWS was the trap here: the clamp put the module straight back inside the one it was
supposed to be clearing, which is an overlap the wall makes unavoidable in that
direction and trivial to avoid in the other.

## 10. in `shift_member_place()`

MEASURED FROM THE ROW THE CALLER ASKED FOR, not from the post-bump row: a bump moves the module
DOWN and costs the group nothing, while going above a blocker or clearing the bottom of the
canvas moves it UP and every other member has to follow by the same amount.

The raise is APPLIED here and the push below still runs, so a single module simply lands on the
fitted row. *shortfall is reported alongside it for the selection, which cannot accept a lift
that only one member got and restores this trial instead.

## 11. in `shift_member_place()`

Topmost overlap, not the first by index. Modules run to twelve rows tall, so a big one dropped
into a packed column lands across two or three neighbours at once and the drop below moves only
what sits at rowAndBelowToDrop or lower - taking the first match by index left the upper
neighbour underneath the module just dropped on it. Index order is creation order, which is why
the same gesture worked on one patch and failed on the next.

## 12. `shift_modules_down()`

Moved here from menus.c so the VST3 plug-in can share it: a module dropped on top of another
must push it out of the way, and menus.c is not linked into the plug-in. Its sibling below has
always lived here, and the two belong together — see canvasDrag.h for the drag that calls them.

send_module_move_msg() tells the G2 where the module went. In the plug-in that reaches a stubbed
msg_send() and does nothing, which is correct: there is no synth attached.

## 13. `shift_selection_down_in()`

THE SELECTION IS LIFTED AS ONE BODY, which is the whole difference between this and running
shift_modules_down() over each member in turn. Members are transparent to each other, so a member
raised on its own to clear the bottom of the canvas is raised THROUGH the members above it and
lands on top of one — the group both deformed and overlapping. Raising every member by the same
amount cannot do that: the shape the user dragged is rigid, so if it did not overlap itself before
the drop it does not overlap itself after.

The same complaint the group clamp in module_drag_motion() was fixed for (CT, 2026-08-30:
"relative position of the group to each other should remain the same. currently, individuals can
reposition vs the rest") — the clamp belongs on the movement, not on the destination.

Trial-and-restore rather than arithmetic: how far a member must rise depends on how far the members
placed before it have already pushed the column, so the only honest way to ask is to place them and
look. Each attempt starts from the recorded positions, so a failed one costs nothing.

## 14. in `shift_selection_down_in()`

The per-member clearance above can still, in principle, drop one member onto another that
the group is transparent to. Cheap to check for certain rather than to argue about, and a
refusal the caller can put back beats a layout the user has to untangle by hand. Only an
increase counts - see count_overlapping_pairs().

## 15. `module_positions_snapshot()`

Both shifts above move modules the user never touched, and undo has to put those back as well as
reversing whatever caused the shift. These two fill in the halves of a tUndoMoveEntry list either
side of the operation: snapshot the location's positions first, then ask which of them moved.
out must have room for MAX_NUM_MODULES entries.

## 16. in `paste_snapshot()`

A paste lands wherever the pointer is, which is very often on top of something. Re-order the
column exactly as a drop does: the pasted modules push whatever they overlap further down, and
are transparent to each other so the pasted block keeps its own shape. Every pasted module is
in gSelection by now, so this is the same call canvas_module_drag_release() makes.
Return value deliberately ignored: a paste into a column already full to MAX_ROWS is the one
case this cannot place, and unwinding a whole paste - modules, cables and the index remap
above - is a bigger job than the case is worth. It leaves the paste where it landed rather
than corrupting the columns around it, which is what the old MAX_ROWS clamp did.
