# Validation sessions - checking engine modules against the G2

How to run a session that compares an engine module with the G2 itself, what to validate next, and the traps that
have already cost time. Written 2026-10-11 after the DrumSynth Kick presets and the Nord Filter were validated in
one afternoon with the method below. Read `capture-inventory.md` for what is already on disk.

**A capture CHECKS the engine; it never sets it.** No law is fitted to a capture. Where the two disagree, the
module's law is settled first, then checked again; a measurement downstream of more than the thing in question is
not evidence (`drumsynth-capture-recipe.md`, top).

## 1. The rig

| | |
|---|---|
| G2 outputs 1-4 | Fireface UC inputs 5-8 (zero-based 4-7), flat. The old desk inputs (a low shelf on 5/6) are history |
| Fireface outputs 0-3 | the G2's In 1-4, for driving its inputs (`tools/tone`) |
| MIDI | TM-1 to the G2; the G2 receives on **channel 3, slot A only** - other slots have no MIDI channel |
| Recording | `tools/capture --device Fireface --out f.wav --seconds N` (8 ch, 96 kHz, 32-bit) |
| Editor | the Debug build, `G2_EDIT_BACKDOOR=1`, ONLINE; `tools/g2bd 'CMD' ...` sends backdoor commands |

**Calibrate every session.** A bare OscB into a 2-Out, captured and rendered (`RENDERWAV`), gives the rig's
offset, engine minus G2: +5.5 dB on 2026-10-09, -0.5 dB on 2026-10-11 - something analogue moves between sessions
(the G2's volume, the interface's gain). An absolute level means nothing without that day's offset. A RATIO within
one capture (a filter's L/R, a band against the body) needs none, which is why the methods below use ratios.

## 2. A session, step by step

1. Start the editor online with the backdoor; `g2bd COMMS` says `online`.
2. `g2bd 'DEVSLOT B' NEWPATCH` - build in a slot whose edit buffer is free (edit buffers may be overwritten freely;
   NEVER store to a bank location without asking). DEVSLOT moves the keyboard focus too, in Patch mode.
3. `ADDMODULE <Name> <col> <row>`, `CABLE VA <from>:<out> <to>:<in>` - read the module indices back with `DUMP`
   (connector numbers are per direction, in `connectorLocationList` order).
4. Settings ON THE G2 with `DEVSET VA <index> <param> <value>` (plain `SET` is local only and measures nothing).
   `PARAMDUMP` before and after: a module switched off by accident once faked a 38 dB difference.
5. Calibration patch first (section 1), then the module.
6. The G2: `capture` while notes are played - `DEVNOTE <note> 127 on|off` (the virtual key: velocity 127 always,
   plays the keyboard-focus slot) or `tools/g2_note "Elektron TM-1" 3 <note> <vel>` (MIDI, any velocity, slot A).
7. The engine: `g2bd 'ENGINEOFFLINE on' 'RENDERWAV <path> <ms> [<note> <vel> [<holdMs>]]' 'ENGINEOFFLINE off'` -
   the same patch, rendered offline to a 48 kHz stereo WAV, while the editor stays online.
8. Analyse with the tools in section 3, record the result in `findings.md`, and update the module's rows in
   `tools/modulestatus.c` (`kEvidence`, `kAspectEvidence`), then regenerate `engine-module-status.md`.

`PUSH` sends the whole slot as one write, if ever the G2 and the editor disagree about a slot.

## 3. Methods and their tools

| What is checked | Rig | Tool |
|---|---|---|
| A filter's response | Noise > filter > 2-Out L, the same Noise > 2-Out R; Kbt and mods off | `filter_response.py --pairs DIR` (L/R per third-octave band, 63 Hz-16 kHz) |
| A percussive module's balance and level | Keyboard Gate > module > 2-Out; several hits by DEVNOTE, one render per hit | `band_balance.py --g2 ... --engine ... --rig <offset>` (bands relative to the body) |
| Harmonic content of a steady tone | module > 2-Out at a known pitch | `harmonics.py` |
| Levels and gains | the module between a known source and a 2-Out, against the calibration patch | RMS with that day's offset |
| A time or rate | a gate train or a clock, the output into something audible | onsets in the capture; align on a signal edge, never on the clock (captures start 0.06-0.6 s late) |
| Which modules matter | the stage patches read off the G2 | `stage_usage.py` (loads each into one slot's edit buffer) |

The outputs are AC-coupled (sound-engine-notes §198): a control signal does not survive to the Fireface as DC.
Measure a control module through something that turns it into sound - an OscA's pitch, a LevAmp's gain.

## 4. Traps (each has cost a session)

- **FFT resolution at low frequencies.** At 4096 points a bin is 23 Hz at 96 kHz (the G2) and 12 Hz at 48 kHz
  (the engine), so third-octave bands below ~300 Hz hold one or two bins laid out differently on each side. It
  showed 2-4 dB on the Nord Filter at Freq 50 that were not there. 16384 points (the tool's default) cured it.
- **A tap before an envelope, in a poly patch,** sums every voice's oscillators, keyed or not (13 voices ~ +11 dB).
  Keep the envelope in the path.
- **Patch mode plays every active slot**, selected or not. A slot left holding a free-running patch sounds under
  the measurement - empty the others or check with a no-note capture.
- **Performance mode is another set of slots**: switching mode swaps the G2's four slots for the performance's.
- **The rig offset moves** between sessions (section 1).
- **Free-running patches vary run to run** (detuned oscillators, LFOs from random phases): two takes of 04 Chris'
  Pad differed by 2.1 dB. Compare averages over several takes, or patches without them.
- **DEVNOTE is velocity 127** and plays the keyboard-focus slot; an unpatched DrumSynth Vel is a fixed 64 units.
- **The G2's outputs roll off** above ~15 kHz (sound-engine-notes §199): compare below 16 kHz, or render with the
  G2 Output Filter on.
- **Check every 2-Out's destination** before a capture: a second module on the same outputs sums in.

## 5. What to validate next

From `stage_usage.py` over the 19 stage patches (2026-10-11): the modules they use most that have never been
compared with the G2. Settings in brackets are the first grid to try.

| Module | Stage patches | Rig and method |
|---|---|---|
| LFO C | 9 | at Hi range into an OscA's pitch or straight out: rate at a few Rate dials per Range (period by zero crossings), each Wave's harmonics at audio rate |
| Glide | 7 | Keyboard Note > Glide > OscA pitch, two MIDI notes legato (ch 3): the pitch's trajectory, Glide 32/64/96, each mode |
| LevMult | 6 | OscA sine into In, a Constant into Mod: output against the Constant (-64, -32, 0, 32, 64) |
| S&H | 6 | OscA (slow) into In, an LFO square as clock, the output into an OscA's pitch: held values and timing |
| ModAmt | 5 | as LevMult, each mode and Inv |
| LP Filter, HP Filter | 4, 2 | `filter_response.py` as the Nord Filter (Freq 50/85, each slope) |
| Envelope ADR | 4 | Keyboard Gate > EnvADR > LevAmp on an OscA: stage times as EnvAHD's check (sound-engine-reference §17.11a) |
| KeyQuant, SwOnOffT | 4 each | through an OscA's pitch / a LevAmp |
| Phaser, LFO B, Envelope D | 3 each | Phaser by `filter_response.py`; the others as LFO C and Envelope ADR |

**One cheap sweep that covers many aspects:** every module's Bypass/On-Off. The audit (`./tools/do-modulestatus
--aspects`) shows almost no Modes / On-Off aspect compared on the G2. A session that toggles Bypass on each module
type in turn, with a source through it, and notes pass, silent or hold settles 134 aspects at once.

Done so far, for reference: DrumSynth Kick 1-5 (bands within 0.5 dB, level within 0.3 dB), Nord Filter (36
settings within 1.44 dB, mean under 0.4), Compressor (8 levels within 0.01 dB) - all 2026-10-11, `findings.md`.
