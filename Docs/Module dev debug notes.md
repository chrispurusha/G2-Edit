  1 Keyboard OK (CT)
  3 4-Out OK (CT)
  4 2-Out OK (CT)
  5 Invert OK (CT)
  7 OscB OK (CT) - Needs wave. Needs values/units checking
  8 OscShpB OK (CT) - Needs values/units checking
  9 OscC (OscC) OK *** FIXED 2026-07-27 (Claude): Tune-mode handling VERIFIED CORRECT (Tune Mode param index 3; all 4 modes render right values Semi 32.0 / Freq 2.09kHz / Factor 6.3516x / Partial 33:1 at raw 96). Also FIXED a layout overlap — the Tune Mode selector menu sat at y=15, overlapping the Tune dial's label; moved to y=5 (top row). AND (same as OscShpB) nudged the Tune dial + mode menu left (x32->29) so the wide Freq/Factor Tune value clears the Cent dial. Verified clear at max value via backdoor.
 12 Reverb OK (CT)
 13 OscString OK
 15 Sw8-1 OK (CT) - needs LEDs verifying
 17 ValSw1-2 Needs more resources
 18 X-fade OK
 19 Mix4-1B OK *** MODIFIED 2026-07-27 (Claude + CT tweaks) — NEEDS (CT) VERIFY: layout reworked — fixed vol-meter overlapping ch4 dial, Exp moved to top-left, connectors/dials respaced (grouped jack+dial), meter repositioned/symmetric
 20 EnvADSR OK *** FIXED 2026-07-27 (Claude): (1) 'D' param was mislabelled "Delay" -> "Decay" (manual + ADSR convention); (2) Sustain showed 0-100% -> now 0-64 'units' via new paramTypeUniPol renderer (raw/2 as N.0/N.5, 127=64.0), per manual "Range: 0 to 64 units" + original editor ParamText::UniPol. A/D/R time table (ADRTimeStrMap) confirmed CORRECT (0.5ms..45s matches manual). Display VERIFIED via backdoor (Sus 64.0/50.0/50.5). NEEDS (CT) VERIFY: nothing critical, but eyeball on hardware
 21 Mux1-8 OK
 22 PartQuant OK *** FIXED 2026-07-27 (Claude): Range now shows bipolar partials 0..+/-63 (was 0-100%). New paramTypePartials renderer: value = raw-64, '+' sign on positives, '*' when |v|>32, per manual + original editor ParamText::Sym. Display VERIFIED via backdoor SET across the range (0 / +2 / +32 / +63* / -5 / -32 all correct; asterisk boundary confirmed both ways). NEEDS (CT) VERIFY only: default changed 100->64 (=0 centre) — confirm hardware's real creation default
 23 ModADSR OK
 24 LfoC OK *** MODIFIED 2026-07-27 (Claude) — NEEDS (CT) HARDWARE VERIFY: range selector strMap was mis-ordered vs the wire value; reordered to {Sub,Lo,Hi,BPM} + default 0→1 (Rate Lo), per original editor ParamText::LFORange (G2Editor.c:44594). Resolves the old "Range Sub parameter values" issue. Also confirm a fresh LFO defaults to Rate Lo
 25 LfoShpA OK *** MODIFIED 2026-07-27 (Claude) — NEEDS (CT) VERIFY: range strMap reordered to {Sub,Lo,Hi,BPM,Clk} + default 0→1 (Rate Lo), same fix as LfoC (24)
 26 LfoA OK (CT) - Ultimately might need pectoral shapes
 27 OscMaster OK
 28 Saturate OK
 29 MetNoise OK
 30 Device OK
 31 Noise OK
 32 Eq2Band OK (CT)
 33 Eq3Band OK (CT)
 34 ShpExp OK
 35 Driver OK - fixed In1 connector labelLoc
 36 SwOnOffM OK - fixed Ctrl connector labelLoc
 38 Pulse OK
 40 Mix8-1B OK *** MODIFIED 2026-07-27 (Claude) — NEEDS (CT) VERIFY: fixed vol-meter overlapping Chain label + ch8; Chain→top-right, meter→far-right column (below Chain/above Out), 8 channels shifted left to balance margins
 41 EnvH OK
 42 Delay OK
 43 Constant OK
 44 LevMult OK
 45 FltVowel OK
 46 EnvAHD OK
 47 Pan OK
 48 MixStereo OK *** MODIFIED 2026-07-27 (Claude) — NEEDS (CT) VERIFY: aligned Lvl4-6 dials to their Pan/In columns (were 2% right, breaking vertical alignment + pitch); moved master level dial right so its value label clears the stereo meter
 49 FltMulti OK
 50 ConstSwT check paramTypeBipLevel dispatch
 51 FltNord OK
 52 EnvMulti OK - fixed L1-L4 dial labels (were all L1)
 53 SandH OK
 54 FltStatic OK
 55 EnvD - Overlapping components
 56 Resonator OK
 57 Automate OK - fixed Ch (channel) param missing strMap, was showing raw number instead of "1".."16"/"This" (17 options, matching manual's MIDI automation channel description)
 58 DrumSynth OK - fixed Pitch M connector overlap + comment typos
 59 CompLev OK - fixed stale comment numbers
 60 Mux8-1X OK - fixed stale comment number
 61 Clip OK
 62 Overdrive OK
 63 Scratch OK
 64 Gate OK - fixed modeLocationList paramTypeMenu render bug (G1/G2 selectors were invisible) + 2 stale comments; LED/connector crowding deferred (2-row module, LED math unclear)
 66 Mix2-1B OK (CT)
 68 ClkGen OK - fixed stale comment numbers
 69 ClkDiv OK - has harmless dead commented-out line
 71 EnvFollow OK
 72 NoteScaler OK
 74 WaveWrap OK
 75 NoteQuant OK
 76 SwOnOffT OK - fixed Ctrl connector labelLoc
 78 Sw1-8 check labelLocLeft vs Mux8-1X convention
 79 Sw4-1 check labelLocLeft vs Mux8-1X convention
 81 LevAmp OK
 82 Rect OK
 83 ShpStatic OK
 84 EnvADR OK - fixed Decay/Release toggle overlapping its own dial (x52->55) + stale comment (46->84 End)
 85 WindSw LED/connector crowding deferred (2-row module, same as Gate)
 86 8Counter OK
 87 FltLP OK - harmless dead commented-out line, optional cleanup
 88 Sw1-4 check labelLocLeft vs Mux8-1X convention (same family question as Sw1-8/Sw4-1)
 89 Flanger OK
 90 Sw1-2 OK (CT) - Need to work out what the offset box is and how it works. Seems to be 0 for left output, 4 for right box. Boxes also have names. Sw1-4 goes offsets 0, 4, 8, 12. 2 boxes also have names. Should display the names, not Out 1 and Out 2. Might need a new param type. Special type could also display the offset?
 91 FlipFlop
 92 FltClassic OK — REVIEWED 2026-07-27 (Claude): overlaps clean; values OK (Freq Hz, Res via strMap, Env %, dB menu)
 94 StChorus OK (CT)
 96 OscD
 97 OscA Works. Needs new UI element for the Freq control (param 0) which takes into account the value of param 6 Pitch Type.
 98 FreqShift
100 Sw2-1
102 FltPhase — REVIEWED 2026-07-27 (Claude): overlaps clean (Level just clears meter). VALUE FLAG (needs CT/manual confirm): FB & Spread default to 64 (dial centre) but render 0-100 ("50.0") via paramTypeCommonDial — almost certainly BIPOLAR (cf EqPeak Gain uses paramTypeBipLevel, shows 0.0 at centre). If confirmed, switch to a bipolar renderer.
103 EqPeak — REVIEWED 2026-07-27 (Claude): OVERLAP FIXED — "Level" overlapped the volume-meter bar; moved Level dial left (x83->74). Verified clean. Values: Freq Hz OK, Gain (paramTypeBipLevel) correct 0.0 at centre. BW still needs a proper UI element (known, deferred).
105 ValSw2-1
106 OscNoise
108 Vocoder
112 LevAdd
113 Fade1-2
114 Fade2-1
115 LevScaler
116 Mix8-1A
117 LevMod
118 Digitizer
119 EnvADDSR
121 SeqNote OK(CT) possibly need to consider park LED
123 Mix4-1C OK (CT)
124 Mux8-1
125 WahWah — REVIEWED 2026-07-27 (Claude): overlaps clean. VALUE FLAG: "Sweep M" uses paramTypeFreq (shows a Hz value, e.g. 554.4Hz) for what looks like a mod amount — suspicious, verify vs manual/hardware.
126 Name OK (CT)
127 Fx-In OK (CT)
128 MinMax
130 BinCounter
131 ADConv
132 DAConv
134 FltHP — REVIEWED 2026-07-27 (Claude): overlaps clean. Values: Freq Hz OK, Slope menu OK. VALUE FLAG: FreqMod default 64 (centre) renders 0-100 ("50.0") — likely bipolar (same class as FltPhase FB/Spread); confirm.
139 T&H
140 Mix4-1S OK (CT)
141 CtrlSend
142 PCSend
143 NoteSend
144 SeqEvent OK(CT) possibly need to consider park LED
145 SeqVal OK(CT) possibly need to consider park LED 
146 SeqLev OK(CT) possibly need to consider park LED
147 CtrlRcv
148 NoteRcv
149 NoteZone
150 Compress OK (CT)
152 KeyQuant
154 SeqCtr
156 NoteDet
157 LevConv
158 Glide
159 CompSig
160 ZeroCnt
161 MixFader OK (CT) except value needs to be 0 to 100 xx.x. 127=100 166=98.4 62=48.4, 63=49.2, 64=50, 65=50.8 as per Mix4-1C
162 FltComb — REVIEWED 2026-07-27 (Claude): OVERLAP FIXED — "Level" overlapped the meter; raised the meter to top-right (y20->6) so it clears the bottom-row Level dial (Level's slot was too tight to move it — Type box is beside it). Verified clean. VALUE FLAG (deferred): FB default 64 renders "50.0" — likely bipolar (same class as FltPhase).
163 OscShpA
164 OscDual
165 DXRouter
167 PShift
169 ModAHD
170 2-In OK (CT)
171 4-In OK (CT)
172 DlySingleA  OK (CT) 
173 DlySingleB  OK (CT) 
174 DelayDual
175 DelayQuad
176 DelayA
177 DelayB OK (CT) Apart from when Clk is set, dial should show time divisions
178 DlyClock
179 DlyShiftReg
180 Operator
181 DlyEight
182 DlyStereo OK (CT)
184 Mix1-1A OK (CT)
185 Mix1-1S OK (CT)
186 Sw1-2M
187 Sw2-1M
188 ConstSwM
189 NoiseGate
190 LfoB *** MODIFIED 2026-07-27 (Claude) — NEEDS (CT) VERIFY: range strMap reordered to {Sub,Lo,Hi,BPM,Clk} + default 0→1 (Rate Lo), same fix as LfoC (24)
192 Phaser
193 Mix4-1A OK (CT)
194 Mix2-1A OK (CT)
195 ModAmt
196 OscPerc - Overlapping components
197 Status
198 PitchTrack
199 MonoKey
200 RandomA OK - fixed Step reusing Edge's map with range 4 instead of 5 (couldn't reach "0%")
201 Red2Blue
202 RandomB
203 Blue2Red
204 RndClkA check Mode/Dice params (range4, NULL strMap - renders as raw number fallback; couldn't determine correct labels from manual, needs hardware verification)
205 RndTrig check Mode param (same NULL strMap issue as RndClkA)
206 RndClkB check Mode param (same NULL strMap issue as RndClkA)
208 RndPattern OK - fixed missing strMap on Wave mode toggle (was logging "No strMap for module type RndPattern" + fixed Loop being a paramTypeMenu with no strMap (showed raw "15") -> now a proper dial like Step/StepM
