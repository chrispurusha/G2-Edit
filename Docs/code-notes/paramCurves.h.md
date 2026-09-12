# paramCurves.h notes

The longer comments from `paramCurves.h`, moved here 2026-09-12 so the code reads cleanly. The code points at each as `// notes §k`. Verbatim and in file order; each is titled by what it documents.

## 1. `osc_pitch_type_param_index()`

The arithmetic behind the oscillator dials, split out from the renderers that print it so
that the sound engine can derive its pitch and shape from exactly the same numbers the dial
text shows. Changing a curve here changes what you see and what you hear together, which is
the point - the two drifting apart would be invisible until it sounded wrong.

Each takes the raw 0..127 param value. Which of the frequency curves applies is decided by
the module's own PitchType param, whose index differs per module - osc_pitch_type_param_index()
gives it, or -1 for a module that has no such param.

## 2. `tFilterTopology`

Which shape a multi-mode filter is currently producing. FltStatic's FilterType selects among the
first three; FltNord adds the fourth.
Which of the three measured topologies a filter module uses. They are not variants of one
another - see the notes in paramCurves.c.

## 3. `tEnvShape`

An envelope segment's SHAPE - the companion to adr_time_seconds() above, which gives its length.
The Shape param names both halves at once (envShapeStrMap is {LogExp, LinExp, ExpExp, LinLin}),
the first word naming the attack curve and the second the decay and release, so three of the four
fall exponentially and only LinLin is straight throughout.

SHARED SO THE DRAWN ENVELOPE AND THE PLAYED ONE CANNOT DISAGREE - and they did. The engine and the
module face carried the same law with two different sharpness constants, 5.0 against 4.0, so the
curve drawn on an EnvADSR was never quite the curve it played. Nothing announced it, because each
file was self-consistent; it is exactly the drift this file exists to prevent. Both constants were
also wrong: measured on the hardware 2026-08-24, the rise and the fall are not equally curved, so
there are now two of them - see ENV_ATTACK_SHARPNESS and ENV_FALL_SHARPNESS in paramCurves.c.
