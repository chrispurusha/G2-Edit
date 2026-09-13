# Sound engine law changes - the revert record

Every law or behaviour the sound engine changed on the strength of a reading of the instrument's own
logic, with what it was before, so any one of them can be put back if the instrument disagrees. Old
code is quoted where it is short; otherwise `git show <commit>:<file>` has it whole.

Started 2026-09-13 at CT's request ("remember the old values somewhere, in case we need to revert").
Add to it whenever a law is replaced. Hardware checks for each are in to-test.md.

## 2026-09-13

| # | What | Old | New | Old code at |
|---|---|---|---|---|
| 1 | Mono/Legato: key to return to when the sounding key comes up | the NEWEST key still held, chosen by the note stack | the HIGHEST key still held, chosen by the engine (reference §15.2) | `e225d27` `src/noteStack.c` `note_stack_note_off()` |
| 2 | A repeated note-on for a key whose voice is still releasing | reuses that voice (`voice_holding_note()`) | takes a fresh voice; the old release rings on (§15.3) | `e225d27` `src/soundEngine.c` |
| 3 | Poly steal with every voice held | the oldest | the oldest, unless it has the lowest note and the new note is higher (§15.3) | `e225d27` `src/soundEngine.c` `voice_to_allocate()` |
| 4 | Computer-keyboard note entry | one sounding note; its key's release silenced everything (`d757f75`), then a held list returning to the newest key (`e225d27`) | every key passed through to the G2 and the engine | `d757f75`, `e225d27` `src/virtualKeyboard.c` |
| 5 | Patch glide | exponential approach: coefficient `1 - exp(-4.6 / (glideSeconds * fs))`, `glidePitch += coeff * (note - glidePitch)` | constant rate, 12 semitones per glide time (§15.4) | `e225d27` `src/soundEngine.c` `sound_engine_render()` |
| 6 | Pitch input scale | `PITCH_MOD_SEMITONES 12.0` | `64.0` - one unit a semitone (§16.2) | `e225d27` `src/soundEngine.c` |
| 7 | Constant | `CONST_PARAM_BIPOLAR (1)` read as "0 = unipolar"; `v = value / 127`, bipolar `v * 2 - 1` | 0 is Bipolar; `constant_level()`: Bipolar (value - 64)/64, Unipolar value/128, 127 = 1.0 (§16.1) | `e225d27` `src/soundEngine.c` `add_node()`, eNodeConstant |
| 8 | EnvADSR Sustain | `value / 127` | `dial_fraction(value)`, value/128 with 127 = 1 (§16.3) | `e225d27` `src/soundEngine.c` |
| 9 | Envelope curves | `ENV_ATTACK_SHARPNESS 2.83`, `ENV_FALL_SHARPNESS 4.32`; the fall normalised to reach zero at the dial time | rise ln 16, fall ln 100 (40 dB per dial time, a pure exponential in the engine), Log target 16/15 (§17.2) | `35c87a7` `src/paramCurves.c` |
| 10 | Envelope stages | fixed-length ramps on a progress counter (`gEnvProgress`, `gEnvStart`), each restarting its curve from the level it began at | per-sample recurrences on the current level (§17.3); linear attack time rounded to the instrument's increment (§17.1) | `35c87a7` `src/soundEngine.c` `envelope_step()` |
| 11 | Pulse width (Sub, in 96 kHz samples) | `exp(2.11883047 + 0.07714113828 d - 0.0000864025056 d^2 + 0.0000004707716063 d^3)`, d = dial | `exp(2.30093 + 8.76455853 x + 0.378462386 x^2 + 0.0289283595 x^3) - 2`, x = dial/127 (§18) | `35c87a7` `src/soundEngine.c` `pulse_time_seconds()` |

| 12 | Patch Vibrato rate | `4.0 + (dial / 127.0) * 4.0` Hz, in the engine snapshot and the Patch Settings display | `vibrato_rate_hz()`: (255 + 256 x dial/127) x (96000/94) / 65536 Hz (§15.6) | `35c87a7` `src/soundEngine.c` `sound_engine_update_from_patch()`, `src/graphics.c` |

API removed along with 1 and 4: `sound_engine_is_polyphonic()` (replaced by `sound_engine_note_sounding()`)
and `note_stack_top()`, both at `e225d27`.

### Reverting one

1-4 are behaviour, not constants: put back the old function from its commit (the note stack's fallback
needs `sound_engine_is_polyphonic()` back as well). 5-11 are a constant or a formula each, quoted
above; 9 and 10 go together, since the recurrences use the new constants.
