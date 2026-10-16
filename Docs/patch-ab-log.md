# Patch A/B log - the engine against the G2, patch by patch

Every patch that has been played on the engine and on the G2 side by side, by ear or by capture, with what was
heard, what was found and where it stands. One row per patch and date; a re-test gets a new row. The method for a
capture is `validation-sessions.md`; a module's own validation belongs in `engine-module-status.md`, not here.

| State | meaning |
|---|---|
| **Matches** | heard or measured alike after the fixes listed |
| **Fixed** | a difference found and fixed; not yet re-heard by CT |
| **Open** | a difference confirmed and not yet explained |
| **Reported** | heard by CT, not yet measured |

Files are the G2 Backups copies CT plays (iCloud `Patches/G2/G2 Backups`) unless noted - they can differ from
the factory bank (AdditiveG2's first variation sends to Bus 1/2 in the backup, Out 1/2 in the factory file).

## 2026-10-16

Measured with a single held note through the virtual key (velocity 127) in slot B, the G2 captured on the
Fireface and the engine rendered with RENDERWAV; held-tone balance in octave bands and the tail after release,
both relative to the held level, so no rig calibration enters.

| Patch | Heard (CT) | Found | State |
|---|---|---|---|
| Bassic FM4 | crackle at note start (plug-in) | a velocity morph on a mixer inside a feedback loop dropped the loop's leg in the morphed copy (notes §192, `build_module_node()`); fixed | Fixed |
| Bassic FM4 | more reverb on the engine | Reverb Dry/Wet law squared where the G2 cubes it (reference §20.5): tail 10-11 dB too high at Dry/Wet 24; now within 2 dB | Fixed |
| BassVaria NL3 | more reverb on the engine | same cause (Dry/Wet 21); tail now within 0.6 dB. Held tone 5-7 dB low above 1 kHz on the engine - not looked into | Fixed / Open |
| BriteTine | possibly more reverb on the engine | same cause (Dry/Wet 45); tail now within 2.3 dB | Fixed |
| BigSweepPad1 | more reverb and more detune/chorus on the G2 | the take was silent on BOTH (note 60, virtual key, slot B): the setup that makes it sound is not yet known | Reported |
| Backtalk DX | an extra higher tone on every note after the first | idle voices struck from 0 where the G2's rest at L4 (reference §14.2a); on the G2 three notes match within 0.7 dB; fixed | Fixed |
| AdditiveG2 | a tone missing on the engine | the backup's variation 1 sends its 2-Out to Bus 1/2 (silent alone on the engine); variation 2 plays all three oscillator groups offline. Not yet captured | Reported |
| basic sequence | a tone missing on variation 3 with the sequencers stopped | not yet measured (the backdoor has no variation command) | Reported |
| Basslines FM4 | engine not as bright | held balance within 2 dB to 1 kHz at velocity 127 (above that the G2 is at the capture's floor). The G2's release has a second, quieter stage (~0.7 s) the engine lacks. Velocity below 127 not tested | Open |

## 2026-10-16 (later) - variations, and the open items

The backdoor now selects a variation (DEVVARIATION). CT's backups of AdditiveG2 and BigSweepPad1 send their 2-Out
to Bus 1/2 on VARIATION 1, so they are silent there on the G2 and on the engine alike; variations 2-8 go to
Out 1/2.

| Patch | Heard (CT) | Found | State |
|---|---|---|---|
| AdditiveG2 (var 2) | a tone missing on the engine | the sync master is in Freq mode with Kbt on; the engine treated Freq as absolute and played it at 329.6 Hz on every key - the G2 tracks the key (reference §6.1a). Now within 1.4 dB in every octave band | Fixed |
| BigSweepPad1 (var 2) | more reverb and detune on the G2 | held tone within 2 dB per octave except 125 Hz (+7 dB, a small component); tail 1-5 dB HIGHER on the engine. Not yet explained | Open |
| basic sequence (var 3, clock off) | a tone missing on the engine | held level and balance agree; after release the G2 rings on at -30 to -58 dB for most of a second, the engine is silent within 0.1 s | Open |
| Basslines FM4 | engine not as bright | held balance within 2 dB; the G2's tail after release is the same kind as basic sequence's. Ruled out by test patches on the G2: the main envelope's release (its own Shape and times), the voice Chorus, polyphony, the delay (cable removed - the tail stays), the bypassed reverb. Take-to-take spread of the tail is large (5-10 dB) | Open |
| BassVaria NL3 | - | the "5-7 dB low above 1 kHz" of the first take is at the capture's noise floor - not a finding | Withdrawn |

