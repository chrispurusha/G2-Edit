# defs.h notes

The longer comments from `defs.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. file scope

ENABLE_LOG_DEBUG is supplied by the Debug build configuration's
Preprocessor Macros in the Xcode project (so it's off in Release, and so
SynthLib's own source files get it too, without needing to include this
header — see contextMenu.c/utilsGraphics.cpp).
```
#define ENABLE_LOG_MODULE_DATA    // Uncomment for module-data logging in any configuration
```

## 2. file scope

Stop dead on something that should not be possible — but only while developing.

A malformed patch section is worth halting for on this side of a release: the log line alone
scrolls past unnoticed, and the point of finding it is to fix it. It is NOT worth taking a user's
editor down for, along with whatever else they had open, when the parse can carry on and lose
nothing more than a few cosmetic names. So a Release build logs and continues where a Debug build
exits at the first sign of trouble.

DEBUG comes from the Debug configuration's Preprocessor Macros in the Xcode project, alongside
ENABLE_LOG_DEBUG. It says "this is a development build", which is the question being asked here —
logging being on is a separate matter.

Always pair it with a LOG_ERROR that says what happened, and always leave the Release path able
to continue: this macro compiles to nothing there, so whatever follows it has to be a real
recovery, not a fall-through into the case it was meant to prevent.

## 3. file scope

TEMPORARY debug aid — mouse crosshair for validating button hit points.
Compiled in for Debug builds only, so it can never reach a release .dmg, and
even then it stays OFF until toggled with F9 at runtime.
To remove entirely: delete this block, render_mouse_crosshair() and its call
in graphics.cpp, toggle_mouse_crosshair() in graphics.h, and the F9 branch in
mouseHandle.c.

## 4. `RADIO_FACE_WIDTH_PERCENT`

Seven characters, PROTOCOL_PARAM_NAME_SIZE, the most a Channel Select name can be. Every group is
measured against this so it never changes size as its buttons are renamed.
How much of a module's width a Channel Select group may occupy, measured from the group's own left
edge. Leaves a margin at the right so a full-width group does not touch the border.

## 5. `WAVE_ICON_FIXED_SHAPE`

The FIXED Shape a waveform ICON is drawn at. Not the module's live value, deliberately: all four
shape-oscillator sines are identical at Shape 0, so a live icon would draw the same picture for
every entry in the drop-down and there would be nothing to choose between. Full Shape is no good
either — SymPulse falls silent there and Pulse narrows to a sliver.

## 6. `SUB_COMMAND_SET_MUTATION_LOCK`

CONFIRMED on real hardware 2026-07-15: location(8)/moduleIndex(8)/locked-bool(8) payload,
verified by toggling live then restarting the app to force a fresh patch redump from the
device - the bit came back correctly changed. The payload follows the pattern of the other confirmed module
sub-commands (SET_PARAM 0x40, MOVE_MODULE 0x34, SET_MODULE_COLOUR 0x31), which is where the
location/moduleIndex/value guess came from. (An earlier test looked like the write "didn't stick" - that was a false alarm from
unrelated version-gated defaulting logic in parse_module_list clobbering the freshly-read bit
on every reparse of an old-format patch; that logic has since been removed.)

## 7. `MENU_BAR_HEIGHT`

Persistent in-window menu bar (see src/menuBar.c) — sits above the existing
topbar, which is why every topbar element's Y coordinate has this added
in (topbarControls.def's X macro, and the handful of literal-coordinate
exceptions in graphics.cpp's render_top_bar()).

## 8. `CONNECTOR_HIT_PADDING`

Grown by this much on every side for HIT TESTING only — the circle is drawn at CONNECTOR_SIZE and
is not touched. A connector's click target used to be exactly the circle, so it shrank with the
zoom and nothing else: 17.5 pt across at 100%, but 8.8 at 50% and 4.4 at 25% — and zooming out is
exactly what you do for cable work. A CONSTANT in screen points (rather than a scaled fraction) is
deliberate: it is worth +46% of target at 25% zoom where it is needed and only +11% at 100% where
it is not.

ONE POINT IS THE MOST IT CAN BE. The tightest layout in the tables is the Gate's stacked input
pairs, 7 units apart, which leaves 7 pt of clear space between their edges at 100% and 1.8 pt at
25%. At one point a side that stays clear everywhere except 25% zoom, where the two overlap by
0.2 pt — under half a retina pixel. Any more and neighbouring connectors would genuinely fight,
and the click registry resolves an overlap by taking the most recently registered, so one of a
stacked pair would become unreachable.

## 9. `same_string_storage()`

dst and src are each evaluated EXACTLY ONCE, which they were not until 2026-08-20. The macro used
to expand dst three times (the self-copy guard, the strncpy, the terminator) and src twice, and
carried a note telling every caller that dst "must be a plain array expression with no side
effects — never something like buffer[atomicIndex]". That is a rule a caller has to remember, and
forgetting it is silent: with an atomic index the three expansions can resolve to three DIFFERENT
rows, so the guard checks one buffer, the copy writes a second and the terminator lands in a
third. It had already been paid for once — a Store site in usbComms.c indexing on gSlot had to
have the slot hoisted into a local by hand.

Hoisting into the do-block moves that from the caller's memory into the macro, where it cannot be
got wrong. sizeof(dst) is unaffected: it is compile-time on the array TYPE and never evaluates its
operand, so it still measures the destination and not the pointer it decays to.

Evaluating src once also closes a deadlock that was reachable in principle: the second expansion
of src sat INSIDE the lock, so a src expression that itself used COPY_STRING would have taken this
same non-recursive mutex twice. Both operands are now resolved before the lock is taken.

The self-copy guard matters: strncpy takes restrict-qualified pointers, so copying a buffer onto
itself is undefined behaviour, not a no-op. It is easy to reach by accident whenever a "save back
to the remembered path" hands that same buffer in as the source.
THE SELF-COPY GUARD GOES THROUGH THIS rather than comparing in the macro body. Written inline as
`(const char *)(dst) != (const char *)(src)` it tripped -Wstring-compare at every call site passing
a literal — "result of comparison against a string literal is unspecified" — which is three of this
project's warnings for a comparison that is deliberate and, with a literal, simply always true.
Taking void pointers means the literal has decayed before the comparison happens, so the check is
unchanged and the warning has nothing to fire on. Returns int, not bool: defs.h is included
before <stdbool.h> in some translation units and must not depend on it.
