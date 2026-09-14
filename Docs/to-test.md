G2-Edit - TO TEST

Built, not yet checked against real hardware or a real user session.
Confirmed -> delete the line. Check failed -> move it to todo.md.
Full detail for each is in findings.md, searchable by the wording below.
- ***KEYQUANT KEYBOARD AND DXROUTER ALGORITHM GRAPH (2026-09-13)*** - KeyQuant's twelve notes are
  one octave of keys (the old on/off buttons are no longer drawn); DXRouter draws the selected DX7
  algorithm, its feedback loop orange when Feedback is above 0. Eight algorithms screenshot-checked
  against the DX7 chart. STILL TO CHECK: clicking a key toggles that note on the G2 (and undo), and
  the DXRouter picture against the original editor's for a few algorithms.
- ***DX TEST PATCH, AND OPERATOR/DXROUTER IN THE ENGINE (2026-09-13)*** - PatchTestFiles/DXTest.pch2
  (an E.Piano, algorithm 5) is in Slot A on the G2; the engine plays DXRouter patches (§14). STILL TO
  CHECK: play it on the G2 and on the engine and compare by ear - brightness (FM depth), the feedback
  operator's edge, level with several carriers (Main scaling), decay times - then capture both.
- ***ENVELOPE HANDLES, GRAPH AREAS NO LONGER DRAG THE MODULE (2026-09-13)*** - a handle on each
  timed breakpoint of every envelope graph (time sideways, level up/down where it is a parameter);
  a press on a graph off its handles does nothing. Checked by real drags on EnvADSR. STILL TO CHECK:
  by hand on EnvMulti, EnvADDSR and Operator, undo of a two-axis drag (two steps), and how the handles
  feel on the small EnvD/EnvH graphs.
- ***RANDOM CLOCKS, LEVSCALER AND SW2-1 TIDIED (2026-09-13)*** - RndTrig, RndClkA, RndClkB and
  RndPattern on one face with Seed/Bypass/Out down the right; LevScaler and Sw2-1(M) fixed.
  Screenshot-checked at 1.0 and 0.59. STILL TO CHECK: CT's eye.
- ***COMPRESS GRAPH WITH DRAGGABLE HANDLES (2026-09-13)*** - static curve, three handles that drag
  Thr/Ratio/RefLvl following the pointer, a live dot from the gain-reduction LEDs, the original's dB
  marks, and the three dials reading dB and :1. One real drag checked offline (Thr -12 -> +5 dB); the
  live dot followed the G2's own meter, and the engine's meter now matches the G2's step for step.
  STILL TO CHECK: by hand in every dial mode (rotary, vertical, horizontal), Alt-drag on a handle for
  morph, undo after a handle drag, and the Ratio/RefLvl handles.
- ***OPERATOR REWORKED (2026-09-13)*** - face re-laid by rule, envelope and level-scaling graphs,
  Coarse read the DX7's way (x0.50..x31 with Fine in Ratio, Hz in Fixed). Screenshot-checked with
  scripted values. STILL TO CHECK: Coarse's reading against the G2's own display at a few settings in
  both modes, the graphs following the dials by hand, and CT's eye on the face.
- ***FACES CLEAR OF A 16-W NAME (2026-09-13)*** - NoteQuant, CtrlSend, NoteSend and EnvADR re-laid
  by rule because their labels or graph ran into a sixteen-W name; KeyQuant given room under its
  name. Screenshot-checked at 1.0 and 0.59 with --name-band. STILL TO CHECK: CT's eye.
- ***SHAPER AND EQ GRAPHS (2026-09-13)*** - Clip, Overdrive, Saturate, ShpExp and WaveWrap draw their
  transfer curve, EqPeak, Eq2Band and Eq3band their response. Checked by screenshot at zoom 1.0 and
  0.59 with scripted values. STILL TO CHECK: turn each dial by hand and watch the curve follow, and
  compare against the original editor's graphs side by side.
- ***LEVEL GROUP RE-LAID OUT BY RULE (2026-09-13)*** - all 16 faces (Constant, ConstSwM/T, CompLev,
  CompSig, LevAdd, LevAmp, LevConv, LevMod, LevMult, MinMax, ModAmt, NoiseGate, EnvFollow, Red2Blue,
  Blue2Red) to module-layout-rules.md's "common face": In top-right, Out bottom-right, the 16% grid.
  Screenshot-checked at 1.0 and 0.59. STILL TO CHECK: CT's eye - especially whether one- and
  two-dial faces should stay left-aligned or move their controls towards the I/O column.
- ***DELAYS AND PITCH/FX RE-LAID BY RULE (2026-09-13)*** - DelayDual, DelayQuad, DlyEight,
  DlyShiftReg, DlyClock and DelayA on DelayB/DlyStereo's template (Range top-left, Time/Clk over the
  Time dial, columns 19/36/53/70, taps ending bottom-right); PShift, FreqShift, Scratch and Digitizer
  on the oscillators' pattern. DlySingleA/B, DelayB and DlyStereo untouched. The range selectors now
  read "Range" on every re-laid face - DlySingleB still says "Slope". Screenshot-checked at 1.0 and
  0.59. STILL TO CHECK: CT's eye.
- ***MIXERS: EXP IN ONE SLOT, MIX2-1 CHANNELS CLEAR OF THE OUT (2026-09-13)*** - only what CT asked
  for, the rest of the approved mixer faces untouched: Exp top-left after the Chain (Mix1-1A/S, and
  "Curve"/Pad top-left on Mix8-1B); Mix2-1A/B channels 8% left and aligned with each other; Mix8-1A
  inputs 5% left, off the meter they overlapped; ModAmt's Exp top-left and its Mod jack beside
  Depth. Screenshot-checked at 1.0 and 0.59. STILL TO CHECK: CT's eye.
- ***ENVELOPE GRAPHS ON ALL NINE ENVELOPES (2026-09-13)*** - EnvADSR's graph generalised to EnvADR,
  EnvAHD, EnvD, EnvH, ModADSR, ModAHD, EnvADDSR and EnvMulti, stages from the manual. STILL TO CHECK:
  EnvADSR still draws exactly as before; each Output Type, EnvADR's Decay/Release with Trig and Gate,
  EnvADDSR's Sustain L1/L2, EnvMulti's Sustain and Reset - against the original editor if possible.
- ***FLTCOMB, FLTPHASE AND VOCODER GRAPHS (2026-09-13)*** - comb teeth from the engine's law, the
  phaser from a model fitted to the 2026-08-29 figures (its Spread law is a placeholder), the
  Vocoder's band routing. Checked by screenshot at zoom 1.0 and 0.59. STILL TO CHECK: by hand, and
  FltPhase's graph against the original editor's at a few Freq and Spread settings.
- ***FLTCOMB PLAYS IN THE ENGINE (2026-09-12)*** - all three Types from the measured laws (§13); by
  meter 10 of 10 settings read the same as the G2. STILL TO CHECK by ear: Freq swept with high FB,
  Deep at full feedback (approximate, §13.4), Kbt, and the FB Mod input (depth unmeasured).
- ***OSCDUAL PLAYS IN THE ENGINE (2026-09-12)*** - pulse, phased saw and a shelved sub-octave (§12).
  By meter one value above the G2 almost everywhere (§12.5). STILL TO CHECK by ear: the sub with Soft
  off and on against the instrument, PW and Phase modulation (depths unmeasured).
- ***EQPEAK, EQ2BAND AND EQ3BAND PLAY IN THE ENGINE (2026-09-12)*** - measured laws (§11), fits
  0.5-0.7 dB; by meter all 16 settings read the same as the G2. STILL TO CHECK: by ear with swept
  dials, Bypass, and a deep wide cut above 1 kHz (§11.5).
- ***FLTMULTI PLAYS IN THE ENGINE (2026-09-12)*** - the DSP code's Chamberlin filter with all three
  outputs (§10); its responses fit the instrument's to 0.5-0.6 dB. By meter, 16 of 19 settings read the
  same as the G2 and the rest are one value apart, each with mixed readings on both sides. STILL TO
  CHECK: by ear with a swept Freq and Res near 127, GComp off, and the Freq and Pitch inputs.
- ***OSCNOISE PLAYS IN THE ENGINE (2026-09-12)*** - two band-passes in series at the pitch, Q from the
  measured Width law (§8). STILL TO CHECK: the narrow end (Width below 80, extrapolated, not resolved)
  by ear against the instrument, and the Width input's depth (from the DSP code).
- ***THE NOISE MODULE PLAYS IN THE ENGINE (2026-09-12)*** - white noise through the Color dial's
  one-pole, corner and level from a 17-point measurement of the instrument. By meter, 6 of 9 Color
  settings read the same as the G2; at the brightest the engine reads one value higher (its noise
  peaks past full scale more often - a crest difference, not level or spectrum). STILL TO CHECK by
  ear: Color swept on a real patch, engine against instrument.
- ***OSCC AND OSCD PLAY IN THE ENGINE (2026-09-12)*** - OscA's oscillator with the waveform read as a
  mode; checked on the G2 as identical to OscA (0.25 dB). STILL TO CHECK by ear in a real patch.
- ***PAN, X-FADE, FADE1-2, FADE2-1 AND MIXSTEREO PLAY IN THE ENGINE (2026-09-12)*** - each law measured
  on the G2 at the converter and checked against the instrument's code; the engine's meters then
  agreed with the G2's on 63 of 63 readings. STILL TO CHECK: the Mod/Ctrl input's depth (taken from
  the instrument's code as 4x the dial range at full attenuator, not yet measured), and by ear, a
  patch that pans or crossfades under modulation.
- ***THE WHOLE MIXER FAMILY PLAYS IN THE ENGINE (2026-09-12)*** - Mix1-1A/S, Mix2-1A/B, Mix4-1A/B,
  Mix8-1A/B and MixFader join Mix4-1C/S, and every Chain input now sounds (it never did). Checked on
  the G2: 106 configurations within 0.07 dB. STILL TO CHECK by ear: a real patch that chains mixers
  or uses Mix8-1B/MixFader, engine against instrument.
- ***ENGINE METERS FOLLOW THE G2's LAW, AND ITS MIXERS THE MEASURED TAPER (2026-09-12)*** - one meter
  value per octave of peak (9, 11, 12 above full scale), and Mix4-1C/4-1S levels as cube + 1%.
  Checked on the instrument: engine and G2 meters agree on 67 of 80 steps of a sweep, the rest one
  value high at boundaries. STILL TO CHECK by eye: a busy patch with the engine on and off - the
  module faces' meters should look the same either way.
- ***G2 ALIKE SAVES ALL FOUR SLOTS, AND PERFORMANCES (2026-09-12)*** - the project now stores each
  slot's patch path and the selected slot, or the .prf2 path in performance mode, where it stored
  only slot A. File > Open takes a .prf2 (all four slots, Perf Mode on), and in Perf Mode File > Save
  writes a .prf2. Projects saved before still open, into slot A. Checked with tools/vst3host
  --dump-state: old path, .prf2, slots A+C with C selected, and a .prf2 with slot B selected all
  come back as saved, and the editor shows the right slot lit; File > Save Perf, clicked in the
  editor, wrote the .prf2 back and it reloads (but see todo.md: the writer changes Morph 8's label
  and a cable filter, in the app too). STILL TO CHECK in Live: save a set
  with patches in A and C and C selected, reopen it; open a .prf2 from File, save it under a new name
  and open that in G2-Edit.
- ***SMALL DRAWS NO LONGER ALLOCATE A METAL BUFFER EACH (2026-09-11)*** - SynthLib's Metal backend
  passes any draw of up to 4 KB of vertices (128 vertices) with setVertexBytes, which is most of
  them, instead of creating a buffer per draw call. All three applications and all three plug-ins
  draw through it. Checked: the three plug-in editors draw as before in tools/vst3host, and the
  continuously repainting panels' idle CPU fell (GenBridge 3.5% -> 2.9%, MidiSyncTool 4.8% -> 3.6%).
  STILL TO CHECK: the three applications look exactly as before - text, cables, meters, menus.
- ***G2 ALIKE IS MULTI-INSTANCE (2026-09-11)*** - every instance owns a whole document (all four
  slots, the patch and performance settings) and an engine of its own; nothing is shared but the
  editor's panels. Checked offline: two engines rendering different patches on two threads at once
  are sample-identical to each rendered alone (0 of 204,800 samples differ); tools/vst3host
  --instances 2 loads two connected instances in one process, each editor drawing its own patch;
  the application's output is bit-identical to before (same checksum). STILL TO CHECK in Live: (1)
  two tracks of G2 Alike, a different patch in each, both playing - each must sound only its own;
  (2) both editors open at once, editing each - a dial in one must not move the other; (3) A-D in
  the editor now switch slots (they used to snap back to A) and the engine plays the selected one;
  (4) a patch opened from the editor's File menu is the one the project reopens with - it used to
  be forgotten. Known limits are in todo.md (shared editor panels, no performance playback yet).
- ***NOTES START AT THEIR OWN SAMPLE (2026-09-11)*** - G2 Alike used to start every note at the
  start of its block. tools/vst3host --offset-test N: a note at sample 0, 256 and 400 is heard from
  frame 28, 284 and 428 - the offset plus the engine's fixed 28-frame latency. Worth hearing in
  Live as tighter timing on fast, quantised parts at large buffer sizes.
- ***THE PLUG-IN EDITOR'S POPUPS TAKE THE KEYBOARD (2026-09-11)*** - a filename can be typed into
  File > Save As, and Escape and Enter close dialogs; the plug-in never passed keys to SynthLib's
  popups. Untested by hand: type a name, backspace, arrows, Enter.
- ***THE EDITOR NO LONGER SLOWS DOWN THE LONGER IT IS OPEN (2026-09-11)*** - CT's "does not refresh
  as quickly as standalone". do-plugin compiled the Metal backend without ARC, so every vertex buffer
  leaked: in tools/vst3host, forty seconds of pointer movement took the process from 1.7 GB to 12.7 GB
  and a frame from 3.8 ms to 44 ms (60 frames a second down to 23). Built with ARC: 511 -> 523 MB and
  a flat 3 ms at 60 frames a second over the same run. In Live the meter timer redraws 20 times a
  second while the engine runs, so it leaked whether or not the mouse moved. STILL TO CHECK in Live:
  leave the editor open on a playing patch for a few minutes - it should stay as responsive as the
  application, and Activity Monitor should show Live's memory flat. For numbers, `launchctl setenv
  G2_PLUGIN_FRAME_STATS 1` before starting Live, and Console shows a line a second from the editor.
- ***SHAPER CURVES FROM THE INSTRUMENT'S LAWS (2026-09-13)*** - paramCurves notes §31-§36. STILL TO
  CHECK by ear against the G2 with a sine at full scale: Clip at Level 64 (clips at half scale, much
  gentler than before); ShpStatic Inv x3 (a fast rise, not a cube root); Saturate Curve 4 at full Amount;
  ShpExp x5 at half Amount. And a hot signal (a LevAmp at 4x) into each - it now keeps its level.
- ***ENVELOPE AND PULSE TIMES FROM THE INSTRUMENT'S LAWS (2026-09-13)*** - reference §17, §18. STILL TO
  CHECK by ear against the G2, same patch: (1) an EnvADSR with Decay 64 and Sustain 0 - the tail should
  now match, where the engine's used to run about 6% long; (2) Attack 127 LinExp takes about 50 s, not
  45; (3) retriggering a slow Exp attack part-way up reaches the top much sooner than from zero.
- ***CONSTANT POLARITY AND PITCH-INPUT SCALE CORRECTED (2026-09-13)*** - reference §16. STILL TO
  CHECK, engine against the G2: (1) a Constant set Bipolar at 76 into an OscA's Pitch input, KBT off -
  an octave above E4 (659 Hz), and Unipolar at 76 (38 units) 38 semitones above E4; (2) an
  LFO at full depth into a Pitch input sweeps about five octaves each way on both; (3) a patch with
  vibrato from an LFO into PitchVar - the engine's used to be a fifth as deep.
- ***THE ENGINE VOICES LIKE THE G2 (2026-09-13)*** - reference §15. The engine keeps the keys held
  itself; Mono and Legato go back to the HIGHEST key still held (not the newest); Mono restarts the
  envelopes on every change of note including that return, Legato on neither; a Poly steal spares
  the lowest note; patch glide is constant rate. The computer keyboard now sends every key to both
  targets, so it plays chords in Poly. Checked offline on SimpleLead with a plucked envelope (attack
  0, decay 40, sustain 0), first 100 ms after each event: Mono 0.018 RMS on each new key and 0.0105
  on the return, Legato 0.0008 and below; hold 67, 60, 64, let 64 go -> 67, let 67 go -> 60. Poly, 2
  voices: 48, 60, 72 keeps 48 and drops 60. STILL TO CHECK, engine against the G2 with the same
  patch, from the computer keyboard and a MIDI keyboard: (1) Mono, hold G, play C, play E, let E go -
  G should sound and restart; (2) Legato the same - G without a restart; (3) Poly at 2 voices, hold
  C3 and C4, play C5 - C3 should survive; (4) glide Normal: an octave should take about twice as
  long as a fifth; (5) a chord from the computer keyboard in Poly.
- ***THE PLUG-IN WRAPPERS ARE PER-INSTANCE NOW (2026-09-11)*** - SynthLib's VST3 and AU wrappers were
  reworked so several copies of a plug-in can be loaded at once (a controller finds its own processor
  through the host's connection, not a global), and the saved state moved from "SLP1" to "SLP2".
  Checked offline: auval clean, tools/auhost renders SimpleLead (peak 0.0716) and opens the editor,
  tools/vst3host opens the editor, and SynthLib/plugin/test passes (21 multi-instance checks, state
  round trips). STILL TO CHECK in Live: (1) a set saved by the 2026-09-09..10 build reopens with its
  patch and parameter values - that is the SLP1 read path; (2) automation, the host's generic panel,
  mod wheel and pitch bend; (3) in Logic or GarageBand, the mod wheel and bend again, since the AU now
  queues its MIDI and delivers it inside the render rather than on arrival.
- ***THE AUDIO UNIT (2026-09-09)*** - "G2 Alike.component", aumu G2al CPur, built by ./do-plugin
  from the same sources as the VST3. auval -v aumu G2al CPur passes clean with no warnings, and
  tools/auhost opens its Cocoa editor and renders a note. Ableton confirmed working by CT on the
  day. STILL TO CHECK: Logic and GarageBand, which are the sandboxed hosts - if the plug-in cannot
  reach ~/Documents the patch path yields silence that looks like every other cause of silence.
  Also worth confirming there: automation of the ten parameters, the mod wheel and pitch bend
  (which the AU wrapper maps itself from raw MIDI, where a VST3 host does the mapping), and that a
  saved project reopens playing the same patch.
- ***THE SHARED PLUG-IN WRAPPERS (2026-09-09)*** - the VST3 is no longer its own g2Vst3.cpp; it is
  SynthLib's generic wrapper driven by plugin/g2Plugin.c. Behaviour should be identical, but the
  paths that changed and are not covered by tools/vst3host are: a project SAVED by an older build
  reopening (the state blob has a header now, and a headerless blob is meant to be read as a bare
  patch path), the host's own generic parameter panel, and automation recorded before the change.
- ***THE PLUG-IN'S METERS AND LEDs NOW REDRAW ON THEIR OWN (2026-09-09)*** - the application half of
  this landed on 2026-09-08; the plug-in had no consumer for the engine's dirty flag, so its
  compressor LED and volume meters moved only while the mouse did. Open the editor in a host with a
  patch playing and confirm the compressor's gain-reduction LED and the module volumes move with the
  mouse still. Worth checking too that an LfoShpA LED blinks at the LFO rate, and that closing and
  reopening the editor leaves nothing running - the timer is stopped in -removeFromSuperview and
  -dealloc, and it retains the view through its block.

HARDWARE CHECKS

FROM THE 2026-08-30/31 MEASUREMENT SESSION - engine changes, none of these heard yet
- OscB's Tri is no longer skewed by Shape. BEHAVIOUR CHANGE: any patch using OscB on Tri with
  Shape off zero will sound different, and at the top of the dial it used to be a sawtooth
- Sine2's clamp lowered 0.03 -> 0.005, so it opens fully at Shape 99% (was 4.5 dB short)
- FltNord: GC now modelled, and the borrowed ladder's passband droop cancelled - the module was
  both too loud and drooping where the instrument is flat. Biggest audible change of the set
- OscShpB Shape now reaches the models as 0..1 rather than 0.5..0.99; matters at the BOTTOM of
  the dial, where raw 0 used to render half-shaped instead of a pure sine
- Free-running oscillators, on by default. G2_ENGINE_NO_FREERUN=1 restores note-gated behaviour.
  Confirm a note-off does not interrupt a drone, and that an enveloped patch is unchanged
- Oscillator/LFO phase now ADVANCES while a voice is idle, so the same note played twice no longer
  starts identically - listen for whether repeated bass notes now sound less machine-like
  (a double-advance bug here put a ~3 Hz wah on every oscillator; fixed and owner-confirmed
  2026-08-31, but it is the thing to listen for if anything sounds unsteady)
- ValSw1-2/ValSw2-1 Ctrl Value: now a 0-64 dial (top step reads 64). Confirm the range really is 64,
  not 128, against the G2's own panel - the device ACCEPTED a raw 127 when written directly
- FltHP and FltNord now play in the sound engine - both need an ear, neither is tuned
- FltStatic (2026-09-13) is now FltMulti's filter with its own damping - it had sounded ~1.65 octaves
  high and always LP. Check at Freq 64: LP, BP and HP at Res 0 and 96, GC off and on (reference §10.4)
- Oscillator PitchMod (OscA/B/C, ShpA/B, OscDual, OscNoise) now tapers as the mixers' Exp - 0.13 at
  64, was 0.25. An LFO into PitchVar at PitchMod 64 should now match the G2's vibrato depth
- FltMulti's damping at the top: 0.01 at Res 127, and Res 110 peaks ~1.9 dB lower than before
- EqPeak BW is now 2√2(1 - BW/128) and every EQ gain reaches +18 dB at 127 - small; listen at BW 0 and 127
- EqPeak FREQ is open: the engine uses the displayed 13.75 × 2^(Freq/12), the instrument's own table
  20 × 800^(Freq/127). On the G2: EqPeak Gain 127, BW 64, noise in - does the peak sit at 44 Hz or
  57 Hz at Freq 20, at 8.2 kHz or 6.7 kHz at Freq 110?
- FltLP slope now reaches the engine (was stuck on 1 pole, read from Bypass) - 6 settings, 1-6 poles
- FltLP can now be bypassed at all - its active flag read a parameter that does not exist
- A/B both with G2_FILTER_LEGACY=1, which restores the old behaviour without a rebuild
- FltClassic drawn response now reaches the top of the box at max Res (K_MAX 3.914 -> 4.0) - eyeball it
- soundEngine.c LADDER_K_MAX (4.3) was chosen while the drawn constant was wrongly 3.914 - needs an ear
- LevAmp gain law rewritten from measurement 2026-08-30: silent at dial 0, four segments, 0-4x
- Super-saw ("sup") phase fix - needs an ear
- The envelope attack is the wrong FORM (fixed-duration shaped ramp against the instrument's one-pole
  approach to a target) - not yet changed. Owner reports the G2's attack as softer; confirm by ear
  against a short-attack patch before and after any fix
- FX In now carries BOTH legs of the FX bus. It resolved only leg 0 and copied it to leg 1, so a
  stereo Voice-area output feeding the FX area arrived as its LEFT channel doubled and anything
  panned right was discarded outright. Play a patch with a stereo source into an FX-area Reverb and
  confirm it is fuller and genuinely stereo; owner reported the engine "not as full as the hardware"
- VST3 editor open/close: the plug-in never called gfx_detach_window, so a reopened editor could
  inherit a dead view's Metal layer and crash the host in -nextDrawable (Ableton Live 12.4.5,
  2026-09-06). Open and close the editor a dozen times in one session, on the same instance and on
  several, and confirm it neither crashes nor draws into the wrong window
- The same pass added the per-frame gfx_attach_window() to the plug-in's drawRect that both sibling
  plug-ins already had. Load TWO G2 Alike instances, open both editors, and confirm each draws its
  own canvas rather than one painting into the other
- The app was checked against the SynthLib hardening and renders normally (NSWindow path); the
  plug-in's view path has not been exercised in a host from here
- Filter resonance, raspiness and whole-graph oversampling changes - need an ear
- Sound engine parameter smoothing (zipper noise on Shape sweeps) - needs an ear
- EnvADSR + mixer Channel Mute in the engine
- Load Perf from Bank - fixed from an owner log, needs a confirming run
- Store/Delete/Load at a bank location - implemented, never run on hardware
- Bank backup/restore - restore path confirmed, full-bank restore of a second bank not repeated
- Cable undo/redo across all five popup commands and drag-connect
- Reconnection sync dialogue: confirm it appears every time

UI / VISUAL CHECKS

FROM THE 2026-08-31 DROP-SHIFT FIXES
- Drop a 5-row module (Compress) onto a packed column so it covers two or three neighbours at once,
  and confirm EVERY one of them moves down - the "chorus didn't move down" report
- Undo after any drop that pushed other modules down, and confirm the displaced ones go back too

FROM THE 2026-08-30/31 SESSION
- The 19 ported module faces - owner was hand-adjusting; confirm none regressed
- Middle-anchored buttons now centre on what is DRAWN, not on the table rectangle. Six rows move
  down about 6 px: LfoA and Constant menus, OscC/OscD Wave, Gate G1/G2
- Module right-click info line now reads "name - index N, R rows" (owner confirmed 2026-08-30)
- New Patch now opens with the FX area minimised (was both areas) - confirm it feels right in use
- Response graphs on FltNord, FltLP, FltHP and FltStatic - check against the real modules on hardware
- Escape no longer quits the application
- File > Save vs Save As, and the Save crash fix - confirm it stops recurring
- Backdoor SCROLL and ZOOM commands
- Text rendering: not re-verified on a Retina display since the glyph rewrite
- SynthLib HiDPI scale fix - committed, all three projects pinned
- Module layout cleanup: all 12 mix modules need a (CT) verify
- FltClassic response graph inset
- PartQuant(22) dial range and its changed creation default
- Parameter Overview panel: drag-to-move and Escape still unverified
- Close button moved top-left and drawn as a cross - needs an owner eyeball
- Patch Window Split Bar - step 1 of 4, in SynthLib
- Device busy overlay (gDeviceOpInProgress) - built 2026-07-26, never hardware tested
- Module drop at the BOTTOM of a column no longer piles modules onto row MAX_ROWS - the incoming one comes up by the shortfall instead, a SELECTION is lifted rigidly so the group keeps its shape, and a column with no room refuses the gesture. Single and group paths both backdoor-verified (new SELECTADD + MOVESEL); what is NOT exercised is the real DRAG-DROP rollback, which restores from the drag snapshot rather than MOVESEL's own
- New backdoor commands SELECTADD and MOVESEL - needs an owner eyeball that they belong there
- +/- keys (bare param nudge and Cmd zoom) now follow the KEYBOARD LAYOUT via glfwGetKeyName() instead of the US physical slots - needs a FI/SWE tester: '+' zooms in, '-' (bottom row, beside '.') zooms out, '´' does nothing

CROSS-PROJECT
- EmuUtility gLcd.pixels data race fix - needs an owner eyeball that the LCD still updates cleanly
- FILE BROWSER SCROLLING IN THE PLUG-IN (2026-09-07), reported broken under Ableton. g2_input_scroll()
  never forwarded the wheel to the popups - the application's scroll_event() does it on its first
  line - so the browser list could only be moved by dragging its scrollbar thumb. Now dispatched
  through SynthLib's own popup table, which also gives the bank browser and alert dialog their wheel.
  Check the file browser scrolls with both a trackpad and a wheel, that the canvas behind it does
  NOT move while it is open, and that the bank browser scrolls too.
- WHEEL DELTAS IN THE PLUG-IN were being treated as points unconditionally. AppKit only reports
  points when hasPreciseScrollingDeltas is YES; a traditional wheel reports LINES, so a notch was
  about two pixels of canvas. A notch should now move the canvas the same distance it does in the
  application. Trackpad behaviour should be unchanged - worth confirming it has not got faster.
- PULSE TIME LAW REFITTED (2026-09-07) from 17 hardware widths - pulse_time_seconds() is now a cubic
  in log rather than two endpoints and a constant ratio, and is up to 11% SHORTER than before across
  the middle of the dial. Anything whose timing depends on a Pulse gate will have moved. Needs an ear
  on a patch that uses one, and ideally a re-check of the two shortest dial settings.
- OscShpB TriSaw (2026-09-14): the peak is now the instrument's (1 + raw/128)/2, held two samples from
  the end at the note's pitch - at Shape 127 a saw with a 0.4% fall on low notes where ours had 3%.
  CT heard the G2's 99% saw as brighter: A/B on a low and a mid note, and at Shape 64 (0.75, was 0.737)
- EnvADSR now runs the instrument's own integer envelope (2026-09-14, reference §17.3). Short and mid
  settings are unchanged. Listen at LONG ones: attacks are slower (LogExp 26 s at 112 where the dial
  says 21, and from ~118 it stops at 0.96 and never decays while held; ExpExp 63 s at 127), and
  decays and releases faster near their end (-40 dB in 37 s at 127). Decay already sounded right to CT
- ***STCHORUS NOW THE INSTRUMENT'S OWN CHORUS (2026-09-14, reference §19) - NEEDS AN EAR.*** Replaces
  the three 2026-09-07 rounds. Same shape as before (two opposed taps, triangle, quarter-cycle stereo),
  now exact, with two audible differences:
  - LEVEL: about 2.5-3 dB lower across the dial than the fitted version (unity at Amount 0, confirmed)
  - RATE: nominally 1.453 Hz at Detune 127, and each StChorus now runs at its own rate, up to 25% off
    nominal, as on the instrument - two in one patch should drift against each other.
- ***REVERB NOW THE INSTRUMENT'S OWN NETWORK (2026-09-14, reference §20) - NEEDS AN EAR.*** Replaces
  every fitted round (the 2026-09-06 stereo rebuild, the 2026-09-07 Brightness refit). Exact word for
  word against the instrument's DSP code, so what is left is whether that sounds like the G2:
  - all four rooms across Time and Brightness, and Brightness below 48 especially (never measured cleanly)
  - DryWet end to end: the law is now squared, where the fitted one was cubed
  - a STEREO source: the dry path now keeps L and R apart (it was the mono average on both sides), and
    a source into L only now has its dry on the left only - as the instrument's code has it
  - Reverb bypassed: each input now passes to its own output (it passed the average to both)
- ***FLTCLASSIC NOW THE INSTRUMENT'S OWN LOOP (2026-09-14, reference §21) - NEEDS AN EAR.*** CT heard the
  engine slightly brighter. Now within 75+ dB of the instrument's code on a saw. Compare at high Freq
  (the top octave is where the old one-pole cascade was brightest), with the EnvADSR sweeping it, and at
  high Res with a loud input (the old knee saturated early). Its Pitch input now works (64 semitones/unit)
- FILTER KBT NOW PIVOTS ON E4, note 64 (2026-09-14, reference §21.3). CT found the engine brighter at KBT
  100% with self-oscillation higher than the G2's; at 100% it was 4 semitones high on every note, on every
  filter. Re-run the self-oscillation comparison at KBT 25/50/100% on a few keys
- FltClassic at full Res with no input should now stay silent until pinged, as on the G2 (a float tail
  used to grow into oscillation by itself)
- MODULES THE ENGINE DOES NOT PLAY ARE GREYED OUT while it runs (always in the plug-in). Toggle the engine
  in the app and check the veil appears and goes at once, that greyed modules still edit and drag, and
  that Operators (played via a DXRouter) and Name labels are NOT greyed
- FLTLP AND FLTHP NOW THE INSTRUMENT'S OWN (2026-09-14, reference §22): brighter at high cutoffs than
  before (corners were low, up to 10 dB at 4x the cutoff), and FltLP no longer compresses a loud input.
  A/B at Freq above 100 with a full-scale saw; G2_FILTER_LEGACY=1 brings the old ones back
- ***FLTNORD NOW THE INSTRUMENT'S OWN FILTER (2026-09-14, reference §23) - NEEDS AN EAR.*** A Chamberlin
  pair, not a ladder. FilterType now works (CT: HP came out as low-pass - every type did). GC is now the
  drive x d rather than the old compensation gain (CT found it harsh). Check all four types at 12 and
  24 dB, GC on and off at high Res, and the resonance character against the G2
- FltNord's face graph now draws each FilterType (CT: non-LP types drew as LP) and GC as the drive -
  eyeball all four types at 12/24 dB against the sound
- ***DELAYA/DELAYB NOW THE INSTRUMENT'S OWN TAP (2026-09-14, reference §24) - NEEDS AN EAR.*** Word-exact
  against its code. Audible: the bottom of LP much darker (LP 0 ~50 Hz, was 660 Hz) and LP 127 fully
  open; the first repeat is now filtered too; DryWet squared (the edges of the dial move); HP a
  different two-state filter; 16-bit memory. A/B against the G2 across LP and HP, and with FB near 127
- ***DRONES NO LONGER CUT OFF (2026-09-14, notes §20) - NEEDS AN EAR.*** A voice still sounding after its key is up used to
  fade out after 2 s; it now plays until stolen, as on the hardware. Check a drone patch holds, and that ordinary notes
  still free their voices when they go quiet. Voice 0 also now plays at rest in every patch (notes §179): an Osc
  wired past the envelope to an Out should sound on load with no key; enveloped sounds must stay silent at rest
- ***COMPRESSOR NOW THE INSTRUMENT'S OWN (2026-09-14, reference §25) - NEEDS AN EAR.*** Word-exact against its
  code. At 03 Chris' Lead's settings the old one did not compress at all (it smoothed the signal and never
  saw a peak); now it lifts by the make-up and holds louder notes down as the G2 does. Check the attack and
  release feel, the Level limiter, and the meter
- ***STEREO IN THE FX AREA (2026-09-14) - NEEDS AN EAR.*** CT: Mix4-1S summed to mono, and so did 2-Out and FX
  In. FX In's right leg was being overwritten with its left, and stereo mixers averaged each pair into one
  mono leg. Now L and R stay apart through FX In, Mix4-1S/Mix1-1S and on to 2-Out. Check 03 Chris' Lead's
  chorus/delay/reverb image, and that a mono source into a stereo mixer's L alone stays on the left
- DELAYB'S FB-MOD AND DRYWET-MOD INPUTS NOW WORK (reference §24.6) - they were ignored. In 03 Chris' Lead a
  Constant on both delays' FB mod takes their feedback to zero on the G2, and now in the engine: one repeat
  each, where the engine played several. With a mod input patched, DryWet is linear, not squared
- ***COMPRESSOR REWRITTEN AS A LEVELLER (2026-09-07) - THE BIGGEST BEHAVIOUR CHANGE OF THE DAY.*** It
  was a downward compressor that ignored Ref Level; the instrument drives the signal TOWARDS Ref Level
  and will BOOST when Ref Level is above it, which the old code could never do. Verified against
  hardware to 0.10 dB on the ratio law and exactly on the Ref Level shape. Expect patches whose
  compressor previously did nothing to now do a lot. Listen with Ref Level both above and below the
  signal, and check the threshold still gates cleanly so quiet passages are not lifted into noise.
- ***ENGINE-DRIVEN MODULE METERS (2026-09-07), new feature - needs an eye rather than an ear.*** With
  the sound engine playing, a Compress module's meter should now move by itself, driven by the engine
  instead of the instrument. Check: it moves with the signal; it reads zero below the threshold; it
  does NOT blank other modules' meters when the engine starts; and with the engine STOPPED every meter
  behaves exactly as before. LFO LEDs are wired up too: an LfoShpA's LED should blink at the LFO rate,
  half on and half off, green - and at high rates it should blink rather than sit at a steady half
  brightness. Mix4to1C, Fx-In and the Out modules now drive their own LEVEL meters too - check both
  halves of a stereo pair move, that they fall smoothly rather than flicker, and that they go green
  to yellow around the top of the scale. Every other module still shows the USB value.
- ***OSCA NOW PLAYS (2026-09-07)*** - patches using it rendered silence before. Implemented from the
  module definition and the shared oscillator DSP, NOT yet checked against the osca/ captures that
  have been on disk since an earlier session. Listen for: the three fixed squares (Sqr50/25/10)
  sounding progressively thinner, Tune and Fine tracking as OscB's do, and Kbt behaving. The WAVEFORMS
  are now verified against the captures - all six identified and the three duties confirmed at 50/25/10
  from their harmonic nulls - so what remains for the ear is pitch, Kbt and level.
- ***OSCSHPA NOW PLAYS (2026-09-07)*** - patches using it rendered silence before. Shares OscShpB's
  measured DSP; its parameter indices and the fact its Waveform is a param not a mode were read off
  the instrument. No captures exist for it, so this is unverified: check the six waveforms sound like
  OscShpB's corresponding ones (Sine1-4, TriSaw, and A's sixth should match B's SymPulse), and that
  Shape sweeps them the same way.
- ***THE WHOLE SHAPER GROUP NOW PLAYS (2026-09-07)*** - Clip, Overdrive, Saturate, ShpExp, WaveWrap,
  ShpStatic and Rect all rendered silence before. Only TWO of the seven are known rather than assumed:
  Rect is exact (the manual states all four operations) and ShpStatic's four buttons name their own
  curves. For the rest the SHAPE follows the manual and the DEPTH LAW is a guess, so this wants ears
  first and a capture second. Listen for: Rect's four modes doing what their names say on a sine;
  ShpStatic's Inv x3/Inv x2 brightening and x2/x3 thinning; Overdrive transparent with Amount at 0 and
  the four types getting progressively harder; Saturate lifting quiet material; ShpExp doing the
  opposite; WaveWrap breaking into extra folds as it opens; Clip getting QUIETER as its dial opens
  (that is the module, not a bug - manual p.205). Also check the parameter ORDER, which was read off
  the layout tables and not the instrument: WaveWrap in particular is the one module whose Mod jack
  and mod dial come BEFORE its signal input and its Amount, and if that is wrong its dial will do
  nothing and its Mod jack will be shaping the audio.
- ***REPLACE A MODULE, FILTER GROUP (2026-09-07), new feature.*** Right-click any filter and there is
  now a "Replace with" submenu listing the other thirteen. Picking one swaps the module in place,
  keeping its index, its position, its colour, any name you gave it, and every cable and knob setting
  that has a counterpart on the new module. Check: cables move to the RIGHT sockets rather than to the
  same-numbered ones - FltClassic's fixed pitch input is connector 2 and FltPhase's is connector 4, and
  Vocoder's audio input is connector 1 where every other filter's is 0; a cable with nowhere to go is
  dropped, so replacing an FltClassic with an FltStatic should lose both its pitch modulation cables
  and keep the audio one. Knobs carry by meaning: Freq stays Freq even though it is parameter 0 on an
  FltClassic and parameter 1 on an FltPhase. The drop-down selectors deliberately do NOT carry - an
  FltLP's Slope does not become an FltHP's - because the instrument's own table has no entry for them.
  A taller replacement pushes its column down, and if the column is full the whole swap is refused
  with nothing changed. Undo should put the module AND its cables back in one step. The mapping itself
  is unit-tested; what needs an eye is the menu, the undo, and whether the G2 agrees with the result.
- ***MODULE PALETTE (2026-09-07), new feature - needs a real mouse.*** A "Modules" button in the
  topbar (between Online and Undo) and View > Module Palette both open a band under the topbar: the
  sixteen module groups as a 2x8 grid, and the selected group's modules as tiles carrying their name
  and their connectors as coloured dots. DRAG a tile onto either area to add the module. What needs
  your hands: that the drag feels right; that the ghost - a real module face at the size and place it
  will land - tracks correctly across the split bar and at each zoom; that dropping on the split bar
  or the scrollbars cancels rather than dropping somewhere odd; that the hover preview names the
  module; and that the wheel and the < > arrows scroll the Osc row when the window is narrow enough
  to need it. Toggling the band was hammered from a script across zoom, scroll and split combinations
  without a failure, and a module scrolled under the band is correctly clipped and unclickable.
- ***OFFLINE NEW PATCH NO LONGER HANGS (2026-09-07).*** File > New Patch with no G2 connected used to
  sit on its "New Patch..." overlay until the editor was force quit, and the reset never happened
  either. It now does the reset locally. Check it clears the patch offline, and that ONLINE it still
  pushes to the G2 as before - that path is unchanged but untested since.
- ***DEVICE-OP SAFETY TIMEOUT NOW ACTUALLY FIRES (2026-09-07).*** It was comparing two different
  clocks and had never fired once. Any device operation whose completion never arrives - the G2
  unplugged mid-op is the obvious one - should now clear its busy overlay after five seconds with a
  line in the log, rather than locking the editor. Worth provoking once by pulling the cable during a
  bank operation.
- ***REPLACE WITH NOW COVERS ALL NINETEEN GROUPS (2026-09-08).*** It was Filter-only, which is why it
  looked permanently greyed out - almost nothing you right-click is a filter. Every module except the
  eleven that are in no group (Blue2Red, Red2Blue, DXRouter, Resonator, NoteDet, NoteZone, LevScaler
  and the four hidden ones) now offers its group. Driven through the real GUI on an FltClassic and
  verified: the menu enables, the submenu lists the other thirteen, and the swap keeps the module's
  index, position and user-given name. Cables and knobs carry BY ROLE - FltClassic's Freq (parameter
  0) became FltVoice's parameter 6, and replacing with an FltStatic correctly DROPPED the pitch
  modulation cable it has no input for. Oscillator and envelope swaps checked the same way. What
  needs your ear rather than my eye: whether a swapped module SOUNDS like the settings it inherited.
- ***MODULE COLOUR SELECTOR IN THE PALETTE BAND (2026-09-08), CT's suggestion.*** Swatches to the
  right of the group grid set the colour NEW modules are created in, and the choice persists, which
  is what the instrument does (manual p.61). Verified by clicking a swatch and adding a module - it
  came out in that colour. Check it applies to the right-click Create Module route too, and that the
  standard grey default still behaves for anyone who never touches it.
- ***PALETTE COLOUR SWATCHES, ENLARGED AND EXTENDED (2026-09-08).*** The swatches are now the same
  height as the group buttons and sit on the same two rows. A swatch does two things: it sets the
  colour NEW modules are created in (the drag ghost is drawn in it too, so it previews), and it
  recolours whatever is SELECTED, group selections included. With nothing selected only the first
  applies. The module right-click menu's colour entry now recolours the whole selection as well - it
  used to do only the clicked module even with several selected, which the manual says is wrong.
  Verified by selecting two oscillators and clicking one swatch: both changed. Check the undo puts
  them all back, and that the colours reach a connected G2.
- ***DOUBLE-CLICK A PALETTE TILE TO ADD (2026-09-08)*** - adds below the focused module without a
  drag, which is what the manual describes (p.81). Check it does not also leave a ghost behind, and
  that it lands where you would expect when nothing is focused.
- ***TOPBAR BUTTON RENAMED "Module Bar"*** and it lights green while the band is open.
- ***REPLACE CONFIRMED ON THE INSTRUMENT (2026-09-08)*** - the whole-patch write path had never run
  with hardware attached before. FltClassic to FltNord, then slot B and back to pull the G2's own
  copy: still FltNord. Replacing with an FltStatic kept the two audio cables and dropped the pitch
  modulation one on the DEVICE'S copy too. Column rearrangement checked on a packed column - growing
  a 4-row filter to an 8-row Vocoder pushed everything below down by four with no overlaps. Slot A
  was used and has been put back to SimpleLead. Nothing checked by ear yet.
- ***PALETTE TILES NOW CARRY THE SELECTED COLOUR (2026-09-08)*** - pick a swatch and the whole tile
  row is drawn in it, so the choice previews on the tiles as well as on the drag ghost. Hover is a
  black frame rather than a paler fill, which would have thrown that colour away exactly when the
  pointer is on the tile you are about to pick up. The selected GROUP button is the app's usual green
  with black text, not the blue it started as.
- ***DRAGGING INTO THE FX AREA CONFIRMED (2026-09-08)*** - dropped into both panes and checked where
  each landed. A release on the split bar or the scrollbar gutter between them creates nothing and
  disturbs nothing, which is intended. Worth confirming by hand that the ghost tracks smoothly ACROSS
  the split bar rather than jumping.
- ***ENGINE METERS AND LEDs NOW REDRAW ON THEIR OWN (2026-09-08)*** - they moved only while the mouse
  did, because the audio thread was updating the atomic arrays perfectly and nothing was telling the
  render loop to look. It now raises a flag when a published value actually CHANGES (not every block,
  or the GUI would run flat out in silence) and the loop consumes it, ticking at 50 ms while the
  engine is active. CT confirms volumes now move with no mouse movement. Still worth checking: an
  LfoShpA LED blinking at a high rate, and that a stopped engine goes back to full idle sleep.
- ***PALETTE SWATCHES REGROUPED BY HUE (2026-09-08)*** - the wire order gModuleColourMap is stored in
  interleaves the hues, so walking it directly showed the shades shuffled next to the right-click
  menu's tidy grid. Both now read gModuleColourFamily; the menu keeps six columns so a COLUMN is a
  hue, the band uses two long rows so a hue's four shades sit together. Grey leads.
- ***THE PALETTE BAND NOW DRAWS IN THE VST3 PLUG-IN (2026-09-08)*** - palette.c was in do-vst3's
  source list but nothing called it, so "Module Bar" pushed the canvas down by the band's height and
  left the band itself empty. g2GlDraw.c now calls palette_render() straight after render_top_bar(),
  and g2Input.c routes the band's press/release/motion/wheel in the application's order. Verified in
  tools/vst3host (open, select Filter, drag FltLP out and drop it - module created) and CT confirms
  the band appears. Still to check IN A REAL HOST: the drop, the double-click add, and the wheel
  scrolling a group wider than the window.
- ***COMPRESSOR GAIN STEP AT THE THRESHOLD (2026-09-08)*** - CT: "when the compressor lights a LED
  the effect is quite brutal, and it has audio glitches around it". The leveller law has unity gain
  only where env == RefLvl, so GATING it at the threshold put a step there of the full makeup: +9.0
  dB in ONE SAMPLE at the stock settings (Thr -12, RefLvl 0, Ratio 4:1), up on every note onset and
  -9.0 dB again on the decay, chattering at audio rate whenever the detector sat near the threshold.
  Simulated: worst one-sample gain jump 9.000 dB and an output discontinuity of 0.1423, against a 220
  Hz sine's own largest sample-to-sample step of 0.0102. The detector is now clamped at the threshold
  from below instead - gain = (target / max(env, threshold))^(1-1/ratio), one expression, no branch -
  which drops the same figures to 0.066 dB and 0.0151. Listen for: no click on note onset, no buzz
  while the LED flickers. EXPECT IT LOUDER IN THE GAPS - the makeup now applies all the time, up to
  +9 dB at the stock settings, which is the honest consequence of the law being continuous.
  UNMEASURED and worth one capture: a steady tone BELOW the threshold. This law returns it with the
  makeup on it; a true gate returns it untouched. Nothing captured so far was played quietly enough
  to tell the two apart.
- ***THE PLUG-IN CAN SAVE (2026-09-09)*** - File > Save and Save As now reach a writer; CT confirms
  the dialogue works in Ableton. STILL UNCHECKED: that the .pch2 it writes opens unchanged in the
  application, that File > Open Recent lists what was opened and saved, and that a save into an
  unwritable folder shows the alert rather than failing silently.
