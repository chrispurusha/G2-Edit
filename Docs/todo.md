G2-Edit TODO

Things to do. ONE LINE PER ITEM - keep it that way.
Measurements, reasoning and completed-work narrative go in findings.md, NOT here.
Built-but-unchecked work goes in to-test.md.

CT
- Fix various module presentation and make module presentation have a common approach (dial and button positions etc.).
- Implement more modules in sound engine.
- Wave graphs for PulseOsc, OscNoise, LfoD, Operator, DrumSynth
- Press V to toggle split position vs Voice-Area-only (manual p64) - NOTE <ctrl>V should Paste
- Zoom to Fit from a right click, fitting the area under the cursor
- Separate zoom for VA and FX.

USER REQUESTS (reported 2026-08-22; none blocking)
- Adjustable scrolling and zoom sensitivity in synth settings - both are far too fast
- Add a top-level Edit menu (Undo, Redo, Cut, Copy, Paste, Delete, Paste Params, Select All)
- Nudge arrows on knobs and sliders, for step-by-step mouse/touchpad adjustment
- Yellow module-selection border is not obvious enough; try twice the line width and/or more prominent yellow
- Reconnection dialogue does not always appear
- Open Recent for patches loaded from a bank
- Move Delete Unused Cables out of the cable popup to a top-level menu
- Local mode button: draw a wave across sequencer columns, ultimately as a wave-representation mode
- Add performance keyboard split/layer/zone UI (manual: "Layering Patches")
- Add a dedicated master-clock/tempo panel

MODULES AND GRAPHICS
- Build the layout comparison: per module, how far each control sits from its transformed .rsrc position
- Delay draws a bypass button in the original at CodeRef 3 but stores only 3 params - decide if we want it
- Port the remaining custom graph displays from the original editor (see findings.md for the full 43)
- Draw the waveform graphics from captured samples rather than by hand
- Gate's type selector should be a SYMBOL picker, not two text dropdowns (six 90x26 line drawings)
- Verify the remaining 117 unverified module types against the hardware
- Delay Time dial range in Time mode: owner reports highest raw = 1 s, lowest = 0.01 ms
- Fill the 39 Unknown slots in gModuleProperties (of 209) by sweeping factory banks for type numbers

FILTERS
- FltNord's LP/BP/HP/BR modes are NOT implemented - fltShape is read but the ladder path ignores it
- Try FltNord as a state-variable filter (svf_filter already takes a shape); evidence in findings.md
- FltNord may share FltMulti's filter: the DSP part behind FltMulti (§10.2) has output selections FltMulti does not use (a BR among them) - test that model against FltNord's captures before building another
- Eq2Band/Eq3band Hi Freq: setting 0 sounds at 8 kHz and 1 at 6 kHz, the reverse of eq2BandHiStrMap's names - check what the G2's own display calls them and fix whichever is wrong (the engine follows the sound)
- EqPeak/Eq3band deep wide cuts above ~1 kHz: the instrument's Chamberlin form is unstable there - measure what it actually does (§11.5)
- FltMulti with GComp OFF is unmeasured (the engine takes the drive as unity), as are its Freq and Pitch inputs
- FltNord's PEAK shape still borrows FltClassic's k law; its LEVEL behaviour is now measured and fixed
- Whether FltNord's GC follows the same law on the 12dB slope and on BP/HP/BR is not established
- Measure FltPhase against the Freq dial - notch positions are not yet tied to it
- FltComb Deep fits only to |g| 0.5 with one section (5 dB rms at full feedback) - find its real structure (§13.4)
- FltComb: only two per patch sound in the engine (MAX_COMB_LINES), and at a 192 kHz engine rate the lowest octave of Freq is clamped (COMB_LINE_SAMPLES); FB Mod depth unmeasured
- Write comb and phaser renderers for FltComb and FltPhase; the original draws a graph on both
- Audit for the other half of the FltStatic crash: a -1 "not present" index that some reader does not check
- Fold the engine's node-kind switch into filter_param_map so one list, not two, decides coverage
- FltClassic Res 120 and 127 cannot be fitted as a filter response - it self-oscillates (see findings)
- FltNord at 12dB: BP self-oscillates and HP is contaminated at Res 127 even with GC on - engine model?
- FltNord Res 127 peak reads +70 dB with GC on: not credible, needs a better Q estimator before use
- Q estimator cannot resolve above ~20: log binning is 36/decade, ~65 Hz at 1 kHz (see findings)
- Re-check FltComb FB 127 and FltPhase FB 127 with the level-tracking test, as FltClassic/FltNord were

SOUND ENGINE
- Reverb L/R peak-correlation LAG cannot be matched in an 8-line tank and no tap placement fixes it; only a single shared buffer would - do not tune the taps further
- Audit the other positionally-initialised tables for the tFilterParams trap (see findings.md)
- Extend engine module coverage; recount the supported types, 23 predates the filter work
- Run the engine-vs-hardware diff: both sides can produce the file, the comparison has not been run
- Notes are not sent to the G2 while the local engine is sounding (owner's request)
- FM is not modelled on any oscillator: OscB's and OscC's FmMod input, FM amount and FM Lin/Trk are ignored by the engine
- OscDual's PW (param 11) and its mod amount (param 6) are SWAPPED in G2-Edit's tables: the face labels 6 as PW and 11 as SqrM - fix the face; the engine reads the instrument's order (§12.1)
- OscDual's PW and Phase input depths are unmeasured (scale 1 in the engine) and Sync is not modelled
- OscNoise's Width and WidthMod are SWAPPED in G2-Edit's tables: on the instrument parameter 6 is Width (it widens the band) and 5 is the Width modulation amount - the module tables (and so the face) call them 5 Width, 6 WidthMod. Fix the face and any engine read; measured 2026-09-12
- OscD's face draws a "Pitch" dial at parameter 3, where the module tables have Tune Md (a Semi/Freq/Factor/Partial drop-down) - check against the instrument and fix the face
- OscB's DualSaw renders as eOscWaveSuper in the engine; hardware says it is DblSaw (detune 0.5*Shape)
- tOscWave has no DualSaw and value 4 means Sqr25 on OscA/C/D - the waveform enum needs a per-module map
- Free-run RENDER is gated on the patch having no per-voice envelope; the exact test is "does a node
  reach an Out without passing a gated envelope" - phase already advances for every patch
- Option to reset oscillator phase on note-on, for predictable bass; hardware free-runs, so not default
- Let oscillators free-run rather than only while a note sounds - some patches depend on it; make it configurable
- Sound engine across cores - investigated and deprioritised, kept for later
- OscNoise computes sin, exp and sqrt every sample for every voice (engine load 13% for a two-module patch against 6-8% for Noise) - with nothing patched into Pitch or Width the coefficients are constant and could be computed once per block
- The engine costs ~2% of a core while SILENT (SimpleLead, no notes: 0.62 s CPU per 30 s, output all zero; a 4-voice chord is 2.2 s) - every instance on an idle track pays it. The time is the whole graph running: per-sample parameter smoothing of every node's 12 values, the voice loop, the reverb. A 'sleep when silent' mode (no voice sounding and the post-mix output below a floor for a second) would recover it, but must keep LFO and oscillator phase advancing and let effect tails finish - not a quick change. (Hoisting the per-sample exp() coefficients was tried 2026-09-11 and gained nothing: the compiler already does it)

MEASUREMENT PROGRAMME
- Finish the EnvADSR oracle at ~/Documents/G2EnvTrace: it compiles and runs but outputs zero until the state-block layout and ENV_TIME_TABLES contents are worked out
- Reverb Brightness below dial 48 over-damps the top and is extrapolated, not fitted - both sides stop measuring there, so it needs a quieter capture of the dial's lower third
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


DO NOT RE-TRY (conclusions from completed work — the reasoning is gone from this file, the constraint is not)

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
