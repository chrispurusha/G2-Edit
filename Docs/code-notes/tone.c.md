# tone.c notes

The longer comments from `tone.c`. The code points at each as `// notes §k`.

## 1. file scope

The other half of `capture`: it plays where `capture` records. Written 2026-10-08 to drive the G2's
inputs (Fireface outputs 0-3 are cabled to the G2's In 1-4), when the 2-In's level was settled by
sweeping a sine into In 1 and reading the G2's own 2-In meter (sound-engine-reference §37).

It talks to the HAL through AUHAL at the device's own rate and channel count, as `capture` does, and
writes the sine to the named channels only - every other output of the interface is held at silence,
so nothing else on the desk or the interface is driven.

Build:  cc -O2 -Wall -o tone tone.c -framework CoreAudio -framework AudioToolbox -framework CoreFoundation
```
Usage:  ./tone --list
        ./tone --device Fireface --channels 0 --db -12 --hold 4
        ./tone --device Fireface --channels 0 --db -48,-40,-30,-24,-18,-12,-6,0 --hold 2.5
```

## 2. in `main()`

NOTHING ABOVE 0 dBFS. A float buffer will carry 1.5 to the interface, which then clips it in its own
converter - a square-ish wave into the G2 that looks like the G2 misbehaving. A level above full scale
is refused rather than clamped, so a typo cannot quietly turn into that.

## 3. in `main()`

STEPPED, NOT RE-OPENED. A list of levels plays as one continuous sine whose amplitude changes at each
step, and each step is printed as it starts, so a reading taken over the backdoor can be matched to its
level by time. Re-opening the device per level, which the first sweep did from a script, puts a gap and
a click at every step.
