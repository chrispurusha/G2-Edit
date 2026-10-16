# meterDisplay.c notes

The longer comments from `meterDisplay.c`. The code points at each as `// notes §k`.

## 1. What this file is for

THE CANVAS'S OWN SMOOTHING OF THE METERS AND LAMPS (2026-10-16), applied to whatever the face is fed -
the G2's values over USB or the sound engine's (moduleGraphics.c notes §29) - so both look alike. CT
reported the meters flickering (2026-10-12; findings.md, "Plug-in editor costs audio").

It changes only what is DRAWN. The value itself is the instrument's: the level and its hold bits come
through as sent, and the engine produces those the way the G2 does (sound-engine-reference §1.2). State
is per module and leg in the document (`gMeterDisplay`), so each plug-in instance keeps its own.

## 2. `meter_display_level()`

UP AT ONCE, DOWN A ROW AT A TIME. A rise is shown the frame it arrives. A fall steps one row every
`METER_FALL_STEP_MS` (50 ms), so a reading that drops from 11 to nothing - a note cut, or the metered
voice changing - runs down over about half a second instead of vanishing between two frames. The
instrument's own follower falls slower than that (about half a second per value), so for a steady
signal the step never engages and the meter shows exactly what was sent.

Only the level (the low nibble) is stepped. The hold bits above it (0x10-0x40) pass through untouched:
they ARE the instrument's peak hold, a second long, and smoothing them would invent a second one.

While a meter has not reached its value it raises `gMeterDisplaySettling`; the application's loop and
the plug-in's meter timer take it (`meter_display_take_settling()`) and draw again, so a fall finishes
even when nothing else is asking for frames.

## 3. `meter_display_lamp()`

A LAMP STAYS LIT AT LEAST `LAMP_MIN_ON_MS` (80 ms). A gate or a clock that is high for a few
milliseconds lights between two frames and is never seen; held this long it shows as a flash. A lamp
that goes dark later than that is drawn exactly as sent.
