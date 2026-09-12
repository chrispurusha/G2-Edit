# mutator.h notes

The longer comments from `mutator.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tMutatorLocks`

Quick Lock state. locked/solo are indexed by tMutatorCategory (mutatorCatNone included -
it has no button of its own, but a non-empty solo set implicitly locks it too, matching the
manual: "solo buttons temporarily lock all other parameters, also those not covered by the
other Quick Lock buttons").

## 2. `mutator_is_permanently_locked()`

Classifies a single param into a Quick Lock category. Returns true if the param should never
be touched by any operator (module-level exclusion is handled separately via
tModule.excludeFromMutation - this is the per-param-type "signal type / mute / bypass" rule from
the manual's PERMANENTLY LOCKED PARAMETERS section).

## 3. `mutator_build_schema()`

Walks every active, non-excluded module in Voice + FX areas of the given slot and fills
entries[] with every continuous, non-permanently-locked param found (stable order: location,
module index, param index - same order tModule.param[][] itself uses). Returns the count
written (capped at maxEntries).

## 4. `mutator_chromosome_path()`

Builds the "chromosome" turtle-walk path, as the original Clavia editor draws it: walks the genome two entries at a time - the first (even index)
turns a running heading in degrees (raw byte value minus 63, i.e. centered on a mid-range dial
value), the second (odd index) steps forward by its own raw byte value in that heading. One
unscaled 2D point is written per genome entry (outPoints[0] is always {0,0}); the caller fits
the resulting path to its own rectangle (min/max bounds vary per genome, by design - that's what
makes two genomes' chromosomes visually comparable at a glance).

## 5. `mutator_apply_genome()`

Writes values into the module database at the given variation and pushes each changed value
over USB (send_param_value) - the same call init_params_on_module/action_copy_variation already
use for other variations. pushUndo controls whether each change is also recorded on the undo
stack (true for commits to real variations, false for scratch/audition writes).

IMPORTANT: real G2 firmware only understands variation indices 0-7 (confirmed on hardware
2026-07-15) - there is no live "ninth variation" on the wire. The original Clavia editor's own
"ninth internal variation" is purely a local in-memory scratch slot (never serialized).
So: audition must target whichever variation is presently active on the front panel
(gPatchDescr[slot].activeVariation, 0-7) so hardware actually plays it, exactly like an ordinary
live knob tweak. Before the first audition write, back up that variation's real values (e.g. via
mutator_read_genome into a spare local slot such as index 9, which is fine to use as long as it's
never sent to hardware) so they can be restored (mutator_apply_genome back into the active
variation, pushUndo=false) if the user backs out of the Mutator without committing.
