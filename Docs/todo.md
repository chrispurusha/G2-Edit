G2-Edit TODO

Things to do. ONE LINE PER ITEM - keep it that way.
Measurements, reasoning and completed-work narrative go in findings.md, NOT here.
Built-but-unchecked work goes in to-test.md.

CT (Priority order)
- Implement more modules using the recent methods, especially those where we need graphical representation of wave/filter. 02 Big Pad and 01 Mini Emulator both play as of 2026-09-19 (to-test.md); the next targets have not been chosen
- Envelopes: the other eight now play (reference §17.9) but their KB gate and Reset are not read (EnvADSR's parameter numbers only), and EnvMulti's rise to an intermediate level is a guess - settle both against the instrument's own envelope parts
- Zoom to Fit from a right click, fitting the area under the cursor
- Separate zoom for VA and FX.
- Module wave/filter graphs, what is left of the original's 43: LevScaler, Mux8-1X, RndTrig, SeqA and SeqNote (two). PulseOsc and LfoD carry one in the original but are NOT module types we have - they are among the unfilled slots below, so they cannot be drawn until the modules exist
- CPU: three quarters of it is `eval_node`'s per-node switch, run once per node per voice per oversampled sample (profile in findings 2026-09-19). The cheap wins are taken; skipping a node whose inputs are constant needs the sub-block restructure above to be worth the test that decides it
- `DELAY_LINE_SAMPLES` is sized 2.8 s at a 96 kHz graph, so at a 192 kHz device the longest Time is truncated to 1.4 s - pre-existing, and worse before the rate cap

USER REQUESTS (reported 2026-08-22; none blocking)
- Adjustable scrolling and zoom sensitivity in synth settings - both are far too fast
- Add a top-level Edit menu (Undo, Redo, Cut, Copy, Paste, Delete, Paste Params, Select All)
- Yellow module-selection border is not obvious enough; try twice the line width and/or more prominent yellow
- Open Recent for patches loaded from a bank
- Move Delete Unused Cables out of the cable popup to a top-level menu
- Local mode button: draw a wave across sequencer columns, ultimately as a wave-representation mode
- Add performance keyboard split/layer/zone UI (manual: "Layering Patches")
- Add a dedicated master-clock/tempo panel

MODULES AND GRAPHICS
- DrumSynth's noise filter (reference §39.4): Res may be 4x too resonant and the cutoff table may be the wrong one - needs a native harness of the module's two DSP parts, which also settles the click and the decays
- The rest of the Logic group: 8Counter, BinCounter, ADConv, DAConv and the logic Delay - the four done 2026-09-19 (reference §38) leave these five
- Generate engine-module-status.md from `module_kind()` instead of keeping it by hand - it was wrong about eight modules within a day of them being added (2026-09-19)
- Re-lay out the remaining families by rule (module-layout-rules.md "common face", tools/relayout.py) - Level group done 2026-09-13; next the delays and the pitch/FX group still on port coordinates
- A drag-and-drop layout mode in the editor that snaps to the grid and writes the rows back - for what the rules cannot settle
- Draw the jack-to-dial link as a short graphical line instead of the "-"/"--" connector label (CT) - the labels already mark every pair
- RndPattern's Loop dial reads "11.7" - the percent dial on a 16-step value; it should be a loop count (check the G2's own reading for the offset)
- Faces touching (not overlapping) a 16-W name at 0.59 (face-shots --name-band) - CT's call, most are his: Automate Ctrl, the sequencers' Cycle/Length, Mix2-1B Chain/Exp, the tall mixers' Curve/Exp, DlyEight Range, RndClkB Char, RndPattern Wave, PitchTrack Threshold, OscShpA Wave
- Operator: read Coarse's text off the G2 panel in Ratio and Fixed (DX law assumed), and L/R Depth's range (table 8 values, DX 0-99) - L1 and Level are 0-127, confirmed 2026-08-10
- Sequencer row-chain inputs: try a "Chain" label (value row and trigger row) and keep it only if it fits at zoom 0.59
- Build the layout comparison: per module, how far each control sits from its transformed .rsrc position
- Delay draws a bypass button in the original at CodeRef 3 but stores only 3 params - decide if we want it
- Port the remaining custom graph displays from the original editor - 12 left on modules we have, listed in findings.md (2026-09-13)
- Graphs on EVERY module with a wave or shape where the face has room, not only the original's 43 (CT 2026-09-13) - survey next
- Re-lay out EnvADR, EnvAHD, ModADSR, ModAHD, EnvADDSR and EnvMulti on EnvADSR's pattern, then give their graphLocationList rows EnvADSR's box
- Draw the waveform graphics from captured samples rather than by hand
- Gate's type selector should be a SYMBOL picker, not two text dropdowns (six 90x26 line drawings)
- Verify the remaining 117 unverified module types against the hardware
- Delay Time dial range in Time mode: owner reports highest raw = 1 s, lowest = 0.01 ms
- Fill the 39 Unknown slots in gModuleProperties (of 209) by sweeping factory banks for type numbers

FILTERS
- FltNord's FM-lin and Res-mod inputs are not modelled in the engine (its filter is the instrument's since 2026-09-14, reference §23)
- EqPeak/Eq3band deep wide cuts above ~1 kHz: the instrument's Chamberlin form is unstable there - measure what it actually does (§11.5)
- FltMulti with GComp OFF is unmeasured (the engine takes the drive as unity), as are its Freq and Pitch inputs - NOT in the host tables (the part computes the drive; its starting X frame is all zeros but for a 0.9 at X4, as FltStatic's X3), so this needs the translate-and-run harness
- Noise: the instrument's whole module is now known exactly (reference §7.2a - LFSR, one-pole A with B = (1-A)/4, output x (1 + dial^3/65536)) and the engine still uses the measured table instead. Its level shape follows the measurement to ~1 dB over dials 0-64 and drifts to 6 dB by 127, over a constant 12 dB offset. Play the two against the G2 and adopt the model if it wins - it is exact where the table is fitted
- Measure FltPhase against the Freq dial - notch positions are not yet tied to it
- FltComb Deep fits only to |g| 0.5 with one section (5 dB rms at full feedback) - find its real structure (§13.4)
- FltComb: only two per patch sound in the engine (MAX_COMB_LINES), and at a 192 kHz engine rate the lowest octave of Freq is clamped (COMB_LINE_SAMPLES); FB Mod depth unmeasured
- FltPhase: capture a Freq sweep, a Spread sweep and each Type at FB 96/112/127 - the graph's model is fitted at one Freq and its Spread law is a placeholder (paramCurves.c notes §40)
- Audit for the other half of the FltStatic crash: a -1 "not present" index that some reader does not check
- Fold the engine's node-kind switch into filter_param_map so one list, not two, decides coverage
- FltClassic Res 120 and 127 cannot be fitted as a filter response - it self-oscillates (see findings)
- FltNord at 12dB: BP self-oscillates and HP is contaminated at Res 127 even with GC on - engine model?
- FltNord Res 127 peak reads +70 dB with GC on: not credible, needs a better Q estimator before use
- Q estimator cannot resolve above ~20: log binning is 36/decade, ~65 Hz at 1 kHz (see findings)
- Re-check FltComb FB 127 and FltPhase FB 127 with the level-tracking test, as FltClassic/FltNord were

SOUND ENGINE
- Sound engine across cores - design note at Docs/engine-multicore-design.md (2026-09-19, nothing built). The blocker is the per-sample note grid; the payoff is 3-4x, capped by a fixed 3.7% of a core; JUCE has no design to borrow, only Apple's audio workgroup. Settle where the deficit actually is first
- `reset_node_state()` runs on the AUDIO THREAD on a topology change - 0.28 ms for 02 Big Pad, 0.59 ms for 01 Mini Emulator, 5-11% of a 256-frame budget. Not the break-up, but bulk clearing inside the callback is an RT rule broken; move it to the publisher or do it incrementally
- Engine headroom: no attenuation anywhere for polyphony, so a pad at full voices sits on the rail at the default 0 dB. Decide whether the Out module, the output stage or nothing should scale with voice count - the G2 itself does not clip here
- Voice count: the engine gives a Poly patch voiceCount+1 voices capped at MAX_VOICES (32) - CONFIRMED right (02 Big Pad asks for and gets 14, 2026-09-19) - but the G2 assigns by DSP load and reports what it actually got (findings 2026-08-29, "15 (16)"), so the topbar should show a requested/assigned pair as the original does
- Only the FIRST node a patch morphs on both axes gets a pair table (MAX_PAIR_NODES 1, reference §26.2.3) - raise it if a patch ever needs two
- MonoKey in a POLY patch: all three priorities read the voice being evaluated, which is a guess - it is a monophonic module and the case may not arise (reference §35.1)
- The sustain pedal does not hold keys in the engine (only its morph group moves); the G2 keeps a sustained key held until the pedal lifts (reference §15.5)
- ShpStatic Inv x3/Inv x2: the engine plays exponents 1/3 and 1/2, the 2026-08-24 capture measured 0.49 and 0.65 (the picker icon draws those) - reconcile
- Audit the other positionally-initialised tables for the tFilterParams trap (see findings.md)
- Extend engine module coverage; recount the supported types, 23 predates the filter work
- Run the engine-vs-hardware diff: both sides can produce the file, the comparison has not been run
- Notes are not sent to the G2 while the local engine is sounding (owner's request)
- FM is not modelled on any oscillator: OscB's and OscC's FmMod input, FM amount and FM Lin/Trk are ignored by the engine
- OscDual's PW (param 11) and its mod amount (param 6) are SWAPPED in G2-Edit's tables: the face labels 6 as PW and 11 as SqrM - fix the face; the engine reads the instrument's order (§12.1)
- OscDual's PW and Phase input depths are unmeasured (scale 1 in the engine) and Sync is not modelled
- LFO KBT (LfoA, LfoB, LfoShpA have the control) is not implemented in the engine: the rate ignores the key. The instrument feeds the same key-tracking values the filters use (pivot E4, reference §21.3)
- OscD's face draws a "Pitch" dial at parameter 3, where the module tables have Tune Md (a Semi/Freq/Factor/Partial drop-down) - check against the instrument and fix the face
- OscB's DualSaw renders as eOscWaveSuper in the engine; hardware says it is DblSaw (detune 0.5*Shape)
- tOscWave has no DualSaw and value 4 means Sqr25 on OscA/C/D - the waveform enum needs a per-module map
- Free-run RENDER is gated on the patch having no per-voice envelope; the exact test is "does a node
  reach an Out without passing a gated envelope" - phase already advances for every patch
- Option to reset oscillator phase on note-on, for predictable bass; hardware free-runs, so not default
- Let oscillators free-run rather than only while a note sounds - some patches depend on it; make it configurable
- OscNoise computes sin, exp and sqrt every sample for every voice (engine load 13% for a two-module patch against 6-8% for Noise) - with nothing patched into Pitch or Width the coefficients are constant and could be computed once per block
- The engine costs ~2% of a core while SILENT (SimpleLead, no notes: 0.62 s CPU per 30 s, output all zero; a 4-voice chord is 2.2 s) - every instance on an idle track pays it. The time is the whole graph running: per-sample parameter smoothing of every node's 12 values, the voice loop, the reverb. A 'sleep when silent' mode (no voice sounding and the post-mix output below a floor for a second) would recover it, but must keep LFO and oscillator phase advancing and let effect tails finish - not a quick change. (Hoisting the per-sample exp() coefficients was tried 2026-09-11 and gained nothing: the compiler already does it)

MEASUREMENT PROGRAMME
- Finish the EnvADSR oracle at ~/Documents/G2EnvTrace: it compiles and runs but outputs zero until the state-block layout and ENV_TIME_TABLES contents are worked out
- Measure the rest of the instrument the way the reverb was: EQs, the remaining envelopes
- Shaper group is IMPLEMENTED but only Rect and ShpStatic are known; capture a transfer curve for Clip, Overdrive, Saturate, ShpExp and WaveWrap - one slow full-scale ramp (or a low sine) per mode gives the ENTIRE curve, since all seven are memoryless
- Confirm the shaper parameter and connector ORDER on the instrument: it was read off the layout tables, and WaveWrap's mod dial and Mod jack both come before its signal ones
- FX modules still missing from the engine: Phaser, Flanger, Vocoder, Digitizer, FreqShift, PShift, Resonator, Scratch, WahWah, NoiseGate, the EQs and the rest of the delay family
- Control modules that promote to audio rate and cost almost nothing to add: the level maths (LevAdd, LevConv, LevMod, LevScaler, ModAmt, Invert), the switches and multiplexers, and Blue2Red/Red2Blue (the summing mixers are done; Pan, X-Fade, the faders and MixStereo are in hand)
- Oscillators: the engine covers OscB, OscShpB, OscA and OscShpA. Eight more exist (OscC, OscD, OscDual, OscMaster, OscNoise, OscPerc, OscPM, OscString) and a patch using any of them renders silence. OscNoise, OscC and OscD are done (2026-09-12)
- tools/harmonics.py is BROKEN: fails at import with "No module named 'wav'", so every harmonic analysis is being written from scratch each time
- OscA's harmonic ROLL-OFF is unverified - the osca/ captures look filtered (saw reads -16 dB at h2 against an ideal -6), so a capture with a known patch is needed; waveform identities and pulse duties ARE confirmed
- Compressor UI: draw the settings graphically (threshold, ratio, RefLvl as a transfer curve) - CT's idea 2026-09-07. The live half is DONE: the engine now drives the meter, see findings.md
- Extend engine-driven meters/LEDs beyond the compressor and the LFOs: every other module with a volumeType or LEDs still shows only what the instrument last sent
- Compressor level probe BUILT and validated (see findings.md); now use it for the unmeasured absolute levels: mixer level-dial law, mixer -6/-12 Pad, 4-Out Pad, FxtoIn/2-Out absolute references
- Compressor DETECTOR: test the PEAK-ON-MAX(|L|,|R|) hypothesis - two experiments that need no absolute calibration, see findings 2026-09-08. Feeding both channels vs one must NOT move the trigger point if it is a max; a narrow pulse against a sine at equal internal amplitude separates peak from RMS
- Compressor detector: re-run the sine/saw/square comparison driving the sidechain from Constant through LevAmp rather than from an oscillator - the oscillator's own waveform amplitudes are unknown, which is the free parameter that made the first result look impossible
- pch2csd (MIT) is an independent cross-check for parameter maps - worth diffing against ours

PROTOCOL AND SECOND OPINIONS (each is a code comment needing hardware or a manual, not a code guess)
- usbComms.c:360 - perf-mode ownership, "synth" or "perf"? ambiguous in the protocol
- usbComms.c:3526 - "send data from the editor" for read-only items; a write-support idea
- usbComms.c:3650-3651, 3834, 3980, 4047 - open questions in comments
- protocol.c:1759, mouseHandle.c:164 and :929, moduleGraphics.c:744/751/758, globalVars.c:87
- moduleGraphics.c:685 - can Mode be morphed?
- send_deassign_midi_cc(): CMCtrlDeassign::WriteStream() (0x23) writes a 1-bit field we do not
- send_perf_mode_change() takes ~3 sends (500 ms retries) before the 0x1f response arrives
- SUB_RESPONSE_PARAM_LIST (0x4d): confirm the fix in parse_command_response() is right

VST3
- ./do-uncrustify does not cover plugin/ or SynthLib/plugin/, so the plug-in sources and both format wrappers are unformatted
- ./do-uncrustify rewrites ~2000 lines of src/moduleResources.h as committed (column alignment only, 0 non-whitespace lines) - format it once and commit, or every run leaves that file dirty
- G2 Alike instances share EDITOR state: each has its own document (four slots) and engine since 2026-09-11, but two open editors still share palette.c, menus.c, splitView.c, mutatorUI.c, paramOverlay.c and SynthLib's click regions and popups, plus the panels and drag flags kept out of the document because static tables point at them (gTopbarControls, gPatchSettingsEdit, gPerfSettingsEdit, gPatchParamsEdit, gPatchNotesEdit, gPatchParamRects) - scroll, zoom and an open panel follow you between editors
- Performance playback in the engine: an instance holds all four slots but plays only the selected one; bind one engine per slot (sound_engine_bind_slot()) and mix them, with each slot's keyboard range and channel
- The plug-in build's engine is ~5% slower than the application's (2.24 s vs 2.14 s CPU for 30 s of a 4-voice chord; it was 13% before SE_LOCAL, 2026-09-11). The thread-local read is now ~1% in a profile; the rest is indexing each banked access by a variable instead of the constant 0 - only a per-engine state struct reached through one pointer would recover it
- At most SOUND_ENGINE_MAX_ENGINES (32) G2 Alike instances per process; the 33rd fails to load. Raise it if anyone hits it - unused banks are zero-fill
- g2Menu.c's loaded-patch name is still one per process, so two editors show whichever file was opened last
- Plug-in editors in tools/vst3host own ~350 MB of GPU memory that is NOT this code's (49 x 8 MB 'owned unmapped (graphics)' regions; the backend allocates one 1120x1660 target, its 4x MSAA copy and six small atlases, ~37 MB), and it barely changes with editor size (431 MB at a quarter of the area). The apps show nothing like it (EmuUtility 128 MB total). Check Live's own footprint per editor before chasing - it may be the harness. In G2 Alike it belongs to the FIRST editor: after closing and re-creating the editor 40 times (vst3host --reopen) the process sat at 196 MB, drawing correctly; GenBridge and MidiSyncTool stayed at ~470 MB either way

- tools/auhost is not in the .gitignore and its BINARY is untracked; tools/vst3host's binary IS tracked, so pick one convention
- The Audio Unit's version number is in two places that must agree: G2_AU_VERSION in plugin/g2Plugin.c and AU_VERSION in do-plugin

ARCHITECTURE AND SHARED CODE
- Adopt GenBridge's audio/MIDI selector in place of the Audio Device and MIDI Input flyouts (appMenuBar.c:902/1035) - per-device remembered settings, real channel limits, a None entry; see GenBridge findings 2026-09-02
- ***PRIORITY*** Migrate gParamRectangle[slot][location][index][param] onto the click-region registry
- Popup lifecycle coordinator: two deliberate exclusions remain
- Generalize mousePanels.c / mouseTopbar.c / selection.c into shared SynthLib components
- Page-selection tabs: SynthEdit built its own; unify with G2-Edit's
- Design one shared control-descriptor schema (type, rect+anchor, range/default, string/colour maps)
- Anchor-based layout resolver alongside the hit registry - depends on the schema above
- Evaluate generalizing msgQueue.c + usbComms.c into a transport-agnostic SynthLib comms layer
- Sweep up the missing undos - coverage grew per-feature and is patchy
- Move any remaining TODOs out of the code and into this file
- Variables should be lowerCamelCase, not underscore_separated - too many underscores were generated
- SynthLib file-naming consistency - deferred, cross-repo, coordinate carefully

BUILD
- Cross-platform build (Windows/Linux) - the render backend seam is in place, the rest is not


- ModADSR/ModAHD: the ATTACK and SUSTAIN mod jacks, and all of ModAHD's, are implemented but unmeasured (reference §17.10) - only the decay is checked against the G2

- Never measure an envelope or any other time constant THROUGH a resonant filter. Tracking a
  cutoff sweep with an 80 ms window on a filter at high Res said our ModADSR attack was 4x slow
  (40 ms against 160); measured directly through the module's own VCA on a sine the two agreed
  within 4 ms. The window cannot resolve the attack and the ring smears the edge. Put the envelope
  on a VCA and read the amplitude.

DO NOT RE-TRY (conclusions from completed work — the reasoning is gone from this file, the constraint is not)

- Do NOT change OscA/OscC/OscD's pulse offsets from 0, 0.5 and 0.875 (reference §6.2). The third
  gives a 1/16 duty, not the 10% the manual's "Sqr10" label promises, and the arithmetic looks like
  an off-by-a-bit begging to be 0.8. It is not: a hardware measurement (2026-08-24) and the
  instrument's own three constants (2026-09-20) independently give 0.875.

- Measure performance on the ARTEFACT the owner is running, not on an offline harness. Every
  engine measurement here is built by hand at -O2, as the plug-in is, so they all agreed with the
  plug-in and all disagreed with the standalone the owner could hear breaking up - for a week of
  hypotheses (buffer size, the USB thread, App Nap, efficiency cores, headroom) before anyone ran
  the Debug build and read its own load figure. -O0 is roughly three times slower here. If a
  measurement and a person's ears disagree, reproduce on their build first (2026-09-19).

- The engine's three per-node output-leg loops (the clear at the top of `eval_node()`, the voice sum,
  the mono fan-out) must keep iterating the CONSTANT `NODE_OUTPUTS`. Replacing it with the node's
  real leg count - 2 for almost every kind against the constant 6 - looks like a three-fold cut in
  the hottest stores in the engine and measured **26% SLOWER**, reproducibly (2026-09-19, 02 Big Pad
  at 16 voices: 26.2% of a core to 32.9%). A fixed trip count of 6 lets the compiler unroll and
  vectorise those loops into straight-line SIMD; a variable bound forces a real loop with a branch
  per iteration, and the branch costs more than the stores saved. Do not re-try without reading the
  generated code first.
- Two other "unchanged input" optimisations were measured the same day and are NOT worth their
  state, though neither is wrong: skipping a node's parameter smoothing once its dials have reached
  their targets is +0.6%, and caching `osc_frequency_hz()`'s exp2 per voice and node is +1.1% for
  2 MB of banked arrays. The profile is why - `eval_node`'s per-node switch is 77% of the engine,
  and no constant-factor trim touches it. See findings 2026-09-19.

- The "sudden/random horizontal VA scroll" was the SIDE WHEEL on the owner's new mouse (CT,
  2026-09-19), not a trackpad momentum tail. Do not re-derive the trackpad theory from that symptom.
  The minor-axis filter written for it (SCROLL_AXIS_DOMINANCE, dropping whichever scroll axis was
  under half the other) has been REVERTED: a nudge of a side wheel is a pure-x event, which that
  filter passes straight through, so it never addressed the cause - and it would have cost a real
  diagonal trackpad gesture its minor axis. If horizontal drift is ever seen again on a machine with
  no horizontal scroll device, that is the point at which the trackpad theory becomes worth testing.

- FT_LOAD_FORCE_AUTOHINT in the glyph rasteriser: tried and REJECTED. Crisper, but it changes glyph
  advances, and with canonical-advance positioning it renders "R andomA 1" / "554.4H z". Do not re-try
  without also making get_text_width() use the same hinted advances — which it cannot, as it never sees
  the device size (gZoomFactor). FT_LOAD_TARGET_LIGHT is the deliberate choice: snaps stems vertically
  while leaving horizontal metrics linear.
- Text layout is deliberately NOT driven by the per-size atlases. get_text_width() and everything built
  on it stay on ONE canonical metrics set (gCanonInfo, measured once at GLYPH_REFERENCE_PX) so widths
  stay continuous and boxes fit. The pen advances by the canonical advance; only each glyph's final
  position is rounded. That costs sub-pixel letter spacing and buys pixel alignment — the right trade
  at 6px.
- gMaxAscent/gMaxDescent must stay canonical, and the baseline must be derived from that canonical
  ascent (round(coord.y + gMaxAscent * scaleFactor), rounded ONCE). An earlier version used each
  atlas's OWN rounded ascent, which rounds a second time against a rasterization whose em height only
  approximates the requested one — that is what caused the intermittent "text renders slightly low"
  drop, since the ascent-to-em ratio is not constant across rasterization sizes.
- Rasterizing 1:1 was tried as a fix for small-text mush and made it WORSE. The root cause was the draw
  path resampling every glyph at an arbitrary sub-pixel phase, not the atlas size.
- The 640x360 minimum window size limits INTERACTIVE resizing only, and that is accepted (owner call:
  do not over-complicate). On macOS, Cocoa applies setContentMinSize:/setContentAspectRatio: to user
  drags but not to programmatic setFrame:, so glfwSetWindowSize() bypasses it and a windowWidth pref
  saved below 640 is restored at its saved size. Self-heals as soon as the window is resized by hand.
- EmuUtility's TARGET_FRAME_BUFF_HEIGHT was changed to 1440 (from 1200) to match the other two at 16:9.
  Safe because nothing lays out against that constant — it only drives window creation size, the size
  limit, the aspect lock and the saved-height derivation, while UI scale comes from
  synthlib_scale_init(TARGET_FRAME_BUFF_WIDTH), i.e. WIDTH only. EmuUtility's content is all
  top-anchored, so 16:9 just leaves more empty grey below it.
- The Seq park LED's exclusion from the LED stream is CORRECT and must STAY — it is what keeps the
  module's other real stream LEDs aligned. (See the open Seq park-LED item above for what is still
  unknown.)
- render_volume_meter stays G2-local in moduleGraphics.c. Moving it to SynthLib alongside
  utilsGraphics.c was considered and DROPPED: it is pure G2 domain (tVolumeType /
  tVolumeMeterConfig / volumeMeterStyle* enum + G2 value-bit decode) built entirely on primitives that
  are ALREADY shared. Moving it would couple SynthLib to G2 module semantics — the opposite of the
  commonalise goal.
- mouseHandle.c:1585 — mouse-move calls synthlib_request_redraw() unconditionally on every move. A
  "only redraw if hover state actually changed" guard (noAction) used to exist and was disabled,
  commented out rather than deleted. Re-adding it risks reintroducing whatever bug caused it to be
  pulled (no record of why) for a performance win that is likely marginal on modern hardware. Leave
  alone unless it is actually observed as a real cost.
- The reverse-queue design deliberately keeps coalescing dirty-bits as flags, not messages
  (gotPatchChangeIndication[], gotPerfSettingsChangeIndication, gReDraw, EmuUtility's gNeedLcdFull/
  gNeedLcdDelta/gLcd.refresh/gLeds, SynthEdit's gRescanNeeded/gReconnectRequested, and
  gStateDumpDebounceTicks which is a debounce counter). gRescanNeeded is additionally a wait-break
  condition inside the connect loops' 100ms slices — a flag's job, not a message's.
- EmuUtility's and SynthEdit's MIDI threads poll-drain with eRcvPoll and do NOT block on eRcvWait like
  G2-Edit's USB thread. They are CFRunLoop-driven (CoreMIDI's notify callback is tied to that run loop)
  and have their own time-based cadences; blocking in msg_receive would stall both. Neither app has a
  gToGuiThread (MIDI->UI) — everything they report upward today is a coalescing dirty-bit, so an empty
  reverse queue would be speculative.
- gLcd uses a mutex, not a double-buffer, because deltas mutate the accumulated frame in place so there
  is no clean front/back to swap. The lock is never held over a GL call.
- eMsgCmdScanDevices is COALESCED in EmuUtility's drain (a local bool; the scan runs once after the
  batch) because a rescan is a wanted-state, not an event: one hub plug can fire several
  kMIDIMsgSetupChanged notifications and each scan walks every destination at 15ms stagger.
- module->connector[i].type deliberately stores only the raw base type, never the upRate promotion —
  protocol.c's own upRate-propagation walk depends on it being the permanent declared type. Use
  effective_connector_type(baseType, upRate) at the point of use instead.
- menus.c is NOT a commonalise candidate. The underlying mechanism (tMenuItem, open_context_menu,
  contextMenu.c) is already fully shared via SynthLib. G2-Edit's menus.c is still 2125 lines
  (EmuUtility/SynthEdit's are 26/95), but what remains is legitimately G2-specific domain action logic
  operating on tModule/tParam/tCable, which do not exist in the other two apps.
- Bottom of the module grid is not a hard wall: a module created (or grown by a replace) near row 127 extends past MAX_ROWS instead of being raised or refused - shift_fit_row() guards collisions with other modules but not the grid edge. Pre-existing, affects plain Add Module identically, and needs a decision on whether the last row should be a wall at all
- write_perf_to_file() does not round-trip a .prf2: ArpTrance.prf2 (Version=22, 8456 bytes) saved by it (Version=23, 8108 bytes) reloads with Morph 8's source label "Group 8" shown as "Knob" and the yellow cable-filter button changed - shared by the app and G2 Alike; diff the two files section by section (morph labels, cable visibility) to find what is dropped
- The G2 and the editor DIVERGE on cable deletes (2026-09-12): after ~80 scripted cable edits in one patch, DELCABLE of X-Fade Out -> 2-Out L and R updated the editor but not the G2 - the patch read back from the G2 held both deleted cables plus the new ones into the same inputs, and the G2 went silent while the engine played; repro in findings.md, cause not isolated (edit count, deleting a fanned-out output's cables, or both)
- tools/vst3host crashed once on EXIT (2026-09-09, CT saw it too): EXC_BAD_ACCESS in objc_release, from objc_autoreleasePoolPop in main - an over-release of something the harness holds, at teardown only. Three clean runs since, so intermittent; the plug-in had already returned from every teardown call by then, but rule out the editor view before blaming the harness

## Sound engine - open at 2026-09-14 (session cut short; see findings.md 2026-09-14 OSCSHPB entry)

- DRONES: only ONE voice drones at rest where the hardware runs every voice (notes §179)
- OscShpB Sine3/Sine4 at Shape 120-127: the hardware is a further 0.2-0.65 dB down and its ratio 0.886 at 120 (reference §27.3) - model the top of the dial if it matters
- OscShpB: SymMod (Shape mod input) and the Sync part not yet compared with the engine
- OscDual (§12.5): compare the new code sample for sample with the harness (the offline part harness kept outside the repo, `harness/g2juno.c`: note its increment is HALF the output pitch), mix levels, Soft, PW/phase inputs and over-range PW wrap; then remove the now-unused oversampling path in oscillator_step() and the decimator if nothing else needs them
- OscShpB TriSaw: the two samples beside the peak (harness sign unsettled, §27.5); a hardware capture at a high pitch would settle it
- OscB: Shape mod input, Sync and FM (FmLin) not modelled (reference §6.5)
- Compressor: new §25 port needs an ear (to-test)
- 03 Chris' Lead coverage left: OscShpB waves (above), native check of Mix4-1C/Mix4-1S, clock-synced DelayB uses a fixed 120 BPM

