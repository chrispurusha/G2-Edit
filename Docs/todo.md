G2-Edit TODO

Things to do. ONE LINE PER ITEM - keep it that way.
Measurements, reasoning and completed-work narrative go in findings.txt, NOT here.
Built-but-unchecked work goes in to-test.txt.

CT - PRIORITY
- Continue with audio unit plugins for the siblings
- Claude should be able to pull SynthLib on all the siblings, once I've pushed it. One to remember.
- You'll see that I've been renaming documents from .txt to .md, rename all of them that way
- Plugin mono voicing doesn’t seem to work as per hardware. If I have a relatively fast attack and delay, with no sustain and I press a keyboard key, hold it and press another - doesn’t move to new note. Does on hardware.
- Plugin GUI seems to maybe not refresh as quickly as standalone on Ableton at least.
- EmuUtility and SynthEdit need the MIDI input and output selection on a pop up dialogue, opened from the main menu. Scan should move to that dialogue as an option.
- G2 Alike shouldn't just be single instance. You claimed it's a single instance model.

CT - LOWER PRIORITY
- Wave graphs for PulseOsc, OscNoise, LfoD, Operator, DrumSynth
- Press V to toggle split position vs Voice-Area-only (manual p64) - NOTE <ctrl>V should Paste
- Zoom to Fit from a right click, fitting the area under the cursor

USER REQUESTS (reported 2026-08-22; none blocking)
- Adjustable scrolling and zoom sensitivity in synth settings - both are far too fast
- Add a top-level Edit menu (Undo, Redo, Cut, Copy, Paste, Delete, Paste Params, Select All)
- Nudge arrows on knobs and sliders, for step-by-step mouse/touchpad adjustment
- Yellow module-selection border is not obvious enough; try twice the line width
- Reconnection dialogue does not always appear
- Open Recent for patches loaded from a bank
- Move Delete Unused Cables out of the cable popup to a top-level menu
- Local mode button: draw a wave across sequencer columns, ultimately as a wave-representation mode
- Add performance keyboard split/layer/zone UI (manual: "Layering Patches")
- Add a dedicated master-clock/tempo panel

MODULES AND GRAPHICS
- Build the layout comparison: per module, how far each control sits from its transformed .rsrc position
- Delay draws a bypass button in the original at CodeRef 3 but stores only 3 params - decide if we want it
- Port the remaining custom graph displays from the original editor (see findings.txt for the full 43)
- Draw the waveform graphics from captured samples rather than by hand
- Gate's type selector should be a SYMBOL picker, not two text dropdowns (six 90x26 line drawings)
- Verify the remaining 117 unverified module types against the hardware
- Delay Time dial range in Time mode: owner reports highest raw = 1 s, lowest = 0.01 ms
- Fill the 39 Unknown slots in gModuleProperties (of 209) by sweeping factory banks for type numbers

FILTERS
- FltNord's LP/BP/HP/BR modes are NOT implemented - fltShape is read but the ladder path ignores it
- Try FltNord as a state-variable filter (svf_filter already takes a shape); evidence in findings.txt
- FltNord's PEAK shape still borrows FltClassic's k law; its LEVEL behaviour is now measured and fixed
- Whether FltNord's GC follows the same law on the 12dB slope and on BP/HP/BR is not established
- Pin FltComb's tuning constant: teeth land at nominal/1.67, is it 5/3 or an integer delay length?
- Measure FltPhase against the Freq dial - notch positions are not yet tied to it
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
- Audit the other positionally-initialised tables for the tFilterParams trap (see findings.txt)
- Extend engine module coverage; recount the supported types, 23 predates the filter work
- Run the engine-vs-hardware diff: both sides can produce the file, the comparison has not been run
- Notes are not sent to the G2 while the local engine is sounding (owner's request)
- OscB's DualSaw renders as eOscWaveSuper in the engine; hardware says it is DblSaw (detune 0.5*Shape)
- tOscWave has no DualSaw and value 4 means Sqr25 on OscA/C/D - the waveform enum needs a per-module map
- Free-run RENDER is gated on the patch having no per-voice envelope; the exact test is "does a node
  reach an Out without passing a gated envelope" - phase already advances for every patch
- Option to reset oscillator phase on note-on, for predictable bass; hardware free-runs, so not default
- Let oscillators free-run rather than only while a note sounds - some patches depend on it; make it configurable
- Sound engine across cores - investigated and deprioritised, kept for later

MEASUREMENT PROGRAMME
- Finish the EnvADSR oracle at ~/Documents/G2EnvTrace: it compiles and runs but outputs zero until the state-block layout and ENV_TIME_TABLES contents are worked out
- Reverb Brightness below dial 48 over-damps the top and is extrapolated, not fitted - both sides stop measuring there, so it needs a quieter capture of the dial's lower third
- Measure the rest of the instrument the way the reverb was: EQs, the remaining envelopes
- Shaper group is IMPLEMENTED but only Rect and ShpStatic are known; capture a transfer curve for Clip, Overdrive, Saturate, ShpExp and WaveWrap - one slow full-scale ramp (or a low sine) per mode gives the ENTIRE curve, since all seven are memoryless
- Confirm the shaper parameter and connector ORDER on the instrument: it was read off the layout tables, and WaveWrap's mod dial and Mod jack both come before its signal ones
- FX modules still missing from the engine: Phaser, Flanger, Vocoder, Digitizer, FreqShift, PShift, Resonator, Scratch, WahWah, NoiseGate, the EQs and the rest of the delay family
- Control modules that promote to audio rate and cost almost nothing to add: the level maths (LevAdd, LevConv, LevMod, LevScaler, ModAmt, Invert), the remaining mixers and Pan, the switches and multiplexers, and Blue2Red/Red2Blue
- Oscillators: the engine covers OscB, OscShpB, OscA and OscShpA. Eight more exist (OscC, OscD, OscDual, OscMaster, OscNoise, OscPerc, OscPM, OscString) and a patch using any of them renders silence. OscNoise is the next cheap one - no pitch tracking - but needs a noise source the engine does not have
- tools/harmonics.py is BROKEN: fails at import with "No module named 'wav'", so every harmonic analysis is being written from scratch each time
- OscA's harmonic ROLL-OFF is unverified - the osca/ captures look filtered (saw reads -16 dB at h2 against an ideal -6), so a capture with a known patch is needed; waveform identities and pulse duties ARE confirmed
- Compressor UI: draw the settings graphically (threshold, ratio, RefLvl as a transfer curve) - CT's idea 2026-09-07. The live half is DONE: the engine now drives the meter, see findings.txt
- Extend engine-driven meters/LEDs beyond the compressor and the LFOs: every other module with a volumeType or LEDs still shows only what the instrument last sent
- Compressor level probe BUILT and validated (see findings.txt); now use it for the unmeasured absolute levels: mixer level-dial law, mixer -6/-12 Pad, 4-Out Pad, FxtoIn/2-Out absolute references
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
- ***PRIORITY*** Ship the Metal window-slot fix: commit SynthLib/src/renderBackendMetal.m in the submodule, then bump the pin in all five projects (GenBridge and MidiSyncTool need ONLY the pin)
- MIDI events are applied at block granularity, not sample-accurate
- ./do-uncrustify does not cover plugin/ or SynthLib/plugin/, so the plug-in sources and both format wrappers are unformatted

- SynthLib's AU wrapper is INSTRUMENTS ONLY: an effect needs kAudioUnitProperty_SetRenderCallback, kAudioUnitProperty_MakeConnection and an AudioUnitRender() pull in au_render(). Left unwritten deliberately - write it against a real effect when GenBridge moves over
- Move GenBridge and MidiSyncTool onto SynthLib/plugin/, then delete the G2_VST3_BUILD spelling renderBackendGL.c still accepts for them
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
- gPaletteList and the sixteen static create-module arrays in menus.c are two copies of the same 171 entries - verified identical 2026-09-08, but only one of them should exist; build the menu from the table
- Level-meter SCALE reads low against the instrument's own meters over USB (CT, 2026-09-08). soundEngine.c already flags the 7 dB-per-step law as approximate and eight points as too few; the confound named there is whether the instrument meters peak or RMS. Settle it with a known level rather than by ear: same signal, engine on and off, compare lit segments
- Plug-in: File > Save has no performance branch - a plug-in instance is one patch in slot 0, so write_perf_to_file() is linked but never reached from there
- Plug-in: key and character events never reach SynthLib's popups (no synthlib_popups_dispatch_key/_char in plugin/g2Input.c), so a filename cannot be typed into the Save browser and Escape does not close a dialog
- tools/vst3host crashed once on EXIT (2026-09-09, CT saw it too): EXC_BAD_ACCESS in objc_release, from objc_autoreleasePoolPop in main - an over-release of something the harness holds, at teardown only. Three clean runs since, so intermittent; the plug-in had already returned from every teardown call by then, but rule out the editor view before blaming the harness
