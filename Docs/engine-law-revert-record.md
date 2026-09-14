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

| 13 | ShpStatic curves | `{s^(1/3), s^(1/2), s^2, s^3}` odd power | Inv x3 1-(1-s)^3, Inv x2 1-(1-s)^2 (both held at full scale), x2 s^2, x3 s^3 (paramCurves notes §31) | `35c87a7` `src/paramCurves.c` `shaper_transfer()` |
| 14 | ShpExp | exponent morphed `1 + a(n - 1)`, n = 2..5 | `(1 - a)s + a s^n` (§32) | same |
| 15 | Saturate | `log(1 + k|x|) / log(1 + k)`, k = a x {4, 16, 64, 256} | `(1 - a)s + a(1 - (1 - s)^n)`, n = 3, 5, 7, 9; slope (1 - a) above full scale (§33) | same |
| 16 | Clip threshold | `2^(-6a)`, a = Level/127 | `1 - Level/128` (§36) | same |
| 17 | Shaper headroom and dials | input clamped to full scale; Amount and mod dials over 127 | clamped to 4x full scale (§46); Amount and mod over 128, 127 = 1 (Clip Level over 128 exactly) | `35c87a7` `src/paramCurves.c` `shaper_settings_build()` |
| 18 | Oscillator PitchMod attenuator (OscA/B/C, ShpA/B, OscDual, OscNoise) | `type_ii_attenuator(dial / 127)`, x squared | the mixer Exp taper, `mix_level_gain(dial)`: 0.01x + 0.99x³ (notes §15) | `35c87a7` `src/soundEngine.c` `type_ii_attenuator()` and its two callers |
| 19 | FltMulti damping | `fmax(0.02, 1 - Res/127)` | `1 - 0.99 Res/128`, exactly 0.01 at 127 (§10.2) | `35c87a7` `src/soundEngine.c` `fltmulti_damping()` |
| 20 | FltStatic | `svf_filter()` with f = 2 sin(π g/2), g = 1 - exp(-2π fc/fs), q = 1/`flt_static_q()` = 2(1 - Res/127)²; the filter map left `.shape` and `.gc` at 0, so FilterType and GC were never read | `fltstatic_step()`: FltMulti's filter, F = 2 sin(π fc/fs), d = 1 - Res/128 (floor 0.01), FilterType read, GC drive × d (§10.4) | `35c87a7` `src/soundEngine.c` `filter_step()`, `filter_param_map()`, eNodeFilter in `add_node()` |
| 21 | FltStatic drawn Q | `flt_static_q()`: d = 1 - v/127 | d = 1 - v/128 | `35c87a7` `src/paramCurves.c` |
| 22 | EqPeak BW | `eq_peak_damping((128 - BW)/64)`: 2(2^N - 1)/√2^N | `eq_peak_bw_damping()`: 2√2 (1 - BW/128) (§11.3) | `35c87a7` `src/paramCurves.c` `eq_bands_build()` |
| 23 | EQ gain dials at 127 | (127 - 64) × 18/64 = +17.7 dB | +18 dB (§11.1) | `35c87a7` `src/paramCurves.c` `eq_dial_gain()` |
| 24 | StChorus taps | two taps about a 2.677 ms centre, ±2.628 and ∓1.943 ms × a triangle, Catmull-Rom reads, moved every sample | tap 1 = 505 - 504u, tap 2 = 65 + 378u in 96 kHz samples, 1/32-sample positions, 4-point Lagrange, moved at 24 kHz (§19.1) | `5e40732` `src/soundEngine.c` `chorus_tap()`, `chorus_read()` |
| 25 | StChorus rate | `CHORUS_RATE_MAX_HZ` 1.3905 × Detune/127; every instance started at phase 0.3836 (`CHORUS_PHASE0`) | Detune × 8 × (1 + trim/4) on a 24-bit phase at 24 kHz, 1.453 Hz at 127 nominal; start phase and trim drawn per instance (§19.2) | `5e40732` `chorus_step()` |
| 26 | StChorus mix | dry 0.9542 × (1.4742 - 0.7744x), each tap 0.9542 × 0.7071x, x = Amount/127 (+3 dB at Amount 0) | dry 1 - a/2, each tap a/2, a = Amount/128 (§19.3) | `5e40732` `chorus_tap()` |
| 27 | EnvADSR stages | floating-point recurrences every engine sample with the exact law: attack `level * mul + add` (Log mul = exp(-ln16/N), add = 16/15 (1 - mul); Exp mul = exp(ln16/N), add = (mul - 1)/15; linear 1/N), decay/release `S + (level - S) exp(-ln100/N)`, LinLin `1/N` steps, N = time × fs; the gate acting at once | the instrument's integer recurrence at 24 kHz with table words rounded down; the gate acting from the next tick (§17.3) | `5e40732` `src/soundEngine.c` `env_rates_build()`, `envelope_step()` |
| 28 | OscShpB TriSaw peak | `0.5 + 0.47 x Shape`, Shape = raw/127 (0.97 at the top), at every pitch | `0.5 + Shape x 127/256` (raw/256: 0.996 at the top), and in the engine never closer to the end of the cycle than two samples at the note's pitch (waveModels.c notes §8) | `5e40732` `src/waveModels.c` `wave_trisaw_peak()`, `src/soundEngine.c` `osc_shp_wave()` |
| 29 | Reverb, the whole module | a fitted model: an eight-line feedback tank with Hadamard mixing and six diffusers (`RV_LINES`, `RV_HADAMARD`, `kRvLen`, `kRvDiffuser`), modulated lines (`RV_MOD_DEPTH` 28, `kRvModHz`), `RV_OUTTAPS` 7 output taps per channel on that tank, room scale `kReverbTypeScale` {1, 1.269, 1.5255, 1.6795}, decay in seconds `kReverbDecayBase` {0.045, 0.29, 0.39, 0.32} + `kReverbDecaySlope` {0.02238, 0.04094, 0.06082, 0.08212} × Time, Brightness damping `REVERB_DAMP_MAX` 0.837 / `REVERB_BRIGHT_K` 57.799, wet × `REVERB_WET_GAIN` 0.5002, DryWet as dry and wet ramps each CUBED | the instrument's own network on one ring of delay memory, exact word for word at 96 kHz: positions, coefficients, LFO, the squared DryWet law and every rounding (§20) | `5e40732` `src/soundEngine.c`: the `REVERB_*`/`RV_*` block, `reverb_step()`, `sound_engine_render_reverb_ir()` and the eNodeReverb case in `add_node()` |
| 30 | FltClassic | the shared ladder: `ladder_filter()`, one-pole stages g = 1 - e^(-w), feedback `LADDER_K_MAX` 4.3 x Res/127 from the fourth pole through `ladder_saturate()` (knee `LADDER_KNEE` 0.7); the Pitch input ignored | the instrument's loop: clipped cubic input, two zeros in the middle stages, 8k = Res x 0.03345, third-order pole, Pitch input at 64 semitones (§21) | `aa5336a` `src/soundEngine.c` `filter_step()`, `ladder_filter()`; FltNord and FltLP still run that ladder |

API removed along with 1 and 4: `sound_engine_is_polyphonic()` (replaced by `sound_engine_note_sounding()`)
and `note_stack_top()`, both at `e225d27`.

### Reverting one

1-4 are behaviour, not constants: put back the old function from its commit (the note stack's fallback
needs `sound_engine_is_polyphonic()` back as well). 5-11 are a constant or a formula each, quoted
above; 9 and 10 go together, since the recurrences use the new constants. 12-23 are likewise one
formula each - but reverting 20 also brings back two bugs (the π tuning error and FilterType never
read), so revert only its damping if that is what is in question. 29 is a whole module, not a
constant: restore the block, `reverb_step()`, the IR renderer and the node fields together.
