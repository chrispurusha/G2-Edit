# moduleReplace.c notes

The longer comments from `moduleReplace.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

Replace a module with another of the same group, keeping the cables that have somewhere to go.
The manual describes the feature on p.82 ("REPLACE A MODULE") and again on p.79; the group and
role tables it needs live in moduleResources.h, and Docs/module-replace-design.md explains both.

THE MODULE KEEPS ITS INDEX. That is the whole reason this is a mutation rather than a delete
followed by a create: every cable, knob assignment, MIDI CC assignment and morph in the patch
refers to a module BY INDEX, and handing the replacement a new one would break all of them for
the sake of a swap the user thinks of as editing one module.

EVERYTHING IS DONE LOCALLY AND PUSHED AS ONE WHOLE-PATCH WRITE. Sending the individual edits
instead would be both slower and wrong: a module or cable WRITE is an ADD on the G2, so rewriting
an existing one leaves the device holding it twice, and back-to-back commands race the patch
version bump the device sends asynchronously. One eMsgCmdWritePatch has neither problem.

## 2. in `module_replace_candidates()`

gModuleProperties is indexed by module type, so walking it yields the group's members in
module-type order rather than the order the instrument's own popup uses. That is a difference
worth knowing about but not worth a second table: the popup is short and alphabetical order
would be no closer to the original than this is.

## 3. `carry_params_by_role()`

Carries the knob settings across BY ROLE: an FltClassic's Freq becomes an FltPhase's Freq even
though one is parameter 0 and the other parameter 1. A parameter with no role, or one whose role
the new module does not have, keeps the new module's own default — which is why the defaults are
laid down first and this runs over the top of them.
