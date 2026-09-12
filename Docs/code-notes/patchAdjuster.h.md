# patchAdjuster.h notes

The longer comments from `patchAdjuster.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `tAdjusterKnob`

The Patch Adjuster — the original editor's Tools > Patch Adjuster (manual ch.8, p.113). Eight
knobs that each nudge EVERY parameter of one category across the whole patch at once: "turning
the Attack knob to the left will decrease all attack times in the patch, relative to their
current position... You do not need to track down where the specific parameters are situated in
a complex patch."

NOT THE MUTATOR, despite both being ways to shape a patch without hunting for parameters. The
Mutator generates new patches at random (mutate/cross/interpolate); this is deterministic and
semantic. The manual pairs them — the Adjuster is the last touch after the Mutator turns up
something promising.

NOT PATCH PARAMETERS AND NOT MORPH GROUPS EITHER. The manual is explicit: the knobs "act as
remote, relative editing tools for all parameters of a specific category, wherever they are
located in the patch. Because of their different nature, these knobs cannot be assigned to MIDI
Controllers or to physical knobs on the synth." Nothing here is stored in the patch; only the
parameters the knobs move are.

HOW A KNOB APPLIES (see adjuster_apply()). Every knob runs -50..+50 with 0 at centre, and works from a BASELINE snapshot
of the patch rather than from the live values:
```
    amount > 0:  new = orig + (max - orig) * amount/50      — interpolate toward maximum
    amount < 0:  new = orig * (50 + amount)/50              — interpolate toward zero
    amount == 0: untouched
```
Working from a baseline is what makes the knobs behave the way the manual describes: returning
one to centre restores its category exactly, and several knobs can be off-centre at once without
fighting each other, because each owns a disjoint set of parameters.

COMMITTING: "As soon as you move the focus to another variation or add or remove a module, the
changes will be permanently applied, and the knobs will return to their middle positions." That
is adjuster_note_patch_changed(), which re-takes the baseline and zeroes the knobs.

## 2. file scope

Baseline values, indexed [location][moduleIndex][paramIndex]. Only the active variation is
snapshotted — that is the one the knobs edit, and it is the one whose change commits.
locationMax covers Va/Fx/Morph; the Morph row is never populated, since patch-settings
pseudo-modules carry nothing any of the eight categories claims.
