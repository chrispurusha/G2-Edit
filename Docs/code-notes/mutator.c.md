# mutator.c notes

The longer comments from `mutator.c`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `mutator_is_permanently_locked()`

Manual's "PERMANENTLY LOCKED PARAMETERS": signal-type selectors and mute/bypass buttons on
oscillators, filters and effects. These map directly onto existing tParamType values - no
name-based heuristic needed. paramTypeEnable is deliberately excluded here: it's reused for
legitimate per-step/per-channel content (sequencer step-events, mixer channel enables, KeyQuant
note toggles - confirmed by grepping moduleResources.h), not a module-level bypass switch.
