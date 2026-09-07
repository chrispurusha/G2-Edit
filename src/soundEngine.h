/*
 * The G2 Editor application.
 *
 * Copyright (C) 2026 Chris Turner <chris_purusha@icloud.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __SOUND_ENGINE_H__
#define __SOUND_ENGINE_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif
#include "types.h"

// A local sound engine, so the editor can make a noise on its own — most usefully offline, where
// there is no G2 attached to hear a parameter change on.
//
// FIRST MILESTONE, and the shape of it matters for what you should expect:
//
//   - One module sounds: whichever single OscB is selected on the canvas. Cables are ignored
//     entirely. This is not "the patch playing", it is "that oscillator playing".
//   - Monophonic. One voice, last note wins.
//   - The active variation only, and morphs are not applied.
//
// The oscillator is an ordinary floating-point implementation using the usual published
// techniques — polyBLEP correction at waveform discontinuities, and a plain phase accumulator.
// It is not a model of the G2's own DSP and will not match it sample for sample. It reads the
// same parameter curves the dials display (see osc_* in renderParams.h), so pitch and shape
// agree with what the module shows.
//
// KNOWN GAPS, deliberate at this stage:
//   - Waveform "sup" is approximated by three detuned sawtooths, not the G2's own algorithm.
//   - Shape applies to "squ" (pulse width) and "tri" (symmetry). It does nothing to "sin",
//     "saw" or "sup" yet.
//   - PitchType "Factor" and "Partial" are relative to a master oscillator, which has no meaning
//     with cables ignored; both fall back to being read as "Semi".
//   - "sin" and "tri" are not band-limited. Their partials fall away steeply enough (1/n^2 or
//     better) that aliasing stays well down; "saw" and "squ", which need it, do get polyBLEP.
//   - The pitch, FM, shape and sync inputs are all unconnected by definition, so their
//     modulation-amount knobs (Pitch M, FM, ShpM) have nothing to act on and are ignored.
//   - THE REVERB'S STEREO IS TWO TAP SETS ON ONE TANK, which is the instrument's own arrangement,
//     but the tap POSITIONS are chosen rather than recovered. Scored by the peak of the L/R
//     cross-correlation over lag — the measure that tells a decorrelated pair from a delayed copy,
//     which reading correlation at lag zero does not — the engine sits at 0.124..0.159 against the
//     instrument's 0.124..0.159, and its peak lag scales with the room as the instrument's does.
//     What is not matched is WHERE that peak sits room by room, and the instrument's own lag is not
//     a single number: repeated windows of one capture put Small at both ~677 and ~1250 samples, so
//     there are at least two tap-distance clusters and only one capture to separate them.
//   - THE REVERB'S BRIGHTNESS IS FITTED FROM 48 UPWARD AND EXTRAPOLATED BELOW IT. Sweeps in Small,
//     Medium and Hall settle the dial over 48..112 to about 1.5 dB/s; below 48 the instrument's high
//     band is in the noise and the engine's own decay fit returns nothing usable, so both sides stop
//     measuring in the same place and the dark end over-damps. It wants a quieter capture of the
//     dial's lower third, not another constant.

// Whether the engine is running and holding the audio device.
bool sound_engine_active(void);

// One line saying what the engine is doing, or why it is silent — "Playing OscB", "Select an OscB
// to play", and so on. The Experimental menu shows this under the toggle, because an engine that is
// on but silent otherwise gives no clue which of the several reasons applies. UI thread only.
const char * sound_engine_status_text(void);

// Why there is no vibrato, in one line: whether the keyboard is sending pressure at all, where that
// has left the morph, and whether the patch actually put an LFO into the graph. Those three failures
// all present as silence but need different fixes. UI thread only.
const char * sound_engine_modulation_text(void);

// Opens the audio device and starts rendering. False if the device would not start, in which case
// nothing is held and the engine stays inactive.
bool sound_engine_start(void);
void sound_engine_stop(void);

// Plug-in host entry points: same preparation as sound_engine_start()/stop(), but the caller owns
// the audio device and drives sound_engine_render() itself. audioOutput.c is not involved.
void sound_engine_start_hosted(double sampleRate);
void sound_engine_stop_hosted(void);

// MEASUREMENT ENTRY POINT: renders the Reverb's impulse response alone, with no patch, no voice and no
// audio device. Fills `frames` interleaved STEREO pairs at deviceRate * ENGINE_OVERSAMPLE — pass 48000
// for the 96 kHz the hardware measurements are expressed in, so a delay length is the same integer in
// both. Fully wet, and the delay lines are cleared first, so two renders in one process cannot bleed
// into one another.
//
// This exists so tools/render.c can put the SAME click through this code that tools/measure.py puts
// through the instrument, and tools/analyse_ir.py can then produce the same numbers for both. The
// reverb is mono, so the two channels are identical and their correlation reads 1.000 against the
// instrument's +0.03 — the harness reporting the gap, not a fault in it.
void sound_engine_render_reverb_ir(double deviceRate, uint32_t type, uint32_t timeValue, uint32_t brightValue, float * out, uint32_t frames);

// A morph group's position, 0..1. The G2 has eight, each hard-wired to a source — group 0 is the
// modulation wheel, and morphStrMap in moduleResources.h names the rest. Setting one sweeps every
// parameter that has a morph range recorded for that group between its dialled value and its morph
// target. Called from the MIDI thread.
//
// Returns true if the position actually moved. Unlike pitch bend, a morph is not read by the audio
// thread directly — it is folded into the parameter snapshot, which is only rebuilt on a redraw. So
// a caller that moves a morph MUST ask for a redraw when this returns true, or the change will not
// be heard until something else happens to wake the render loop.
// Output attenuation in dB, 0 or negative. Applied before the output limiter, so it pulls a hot
// patch down rather than leaving the limiter to do it. Positive values are treated as 0 — this is a
// trim, and boosting into the limiter is what it exists to avoid.
void sound_engine_set_output_level_db(double db);

bool sound_engine_set_morph(uint32_t group, double amount);

// Pitch bend, -1..+1 across the wheel's travel. How many semitones that is comes from the patch's
// own Bend setting, so the engine bends by the same amount the G2 would. Called from the MIDI thread.
void sound_engine_pitch_bend(double bend);

// Note input. Called from the UI thread — see set_sounding_note() in virtualKeyboard.c, which is
// the single point every note change goes through.
//
// A note-off NAMES ITS NOTE: the engine releases the voice holding that note and leaves the others
// alone. Passing note < 0 with on == false is all-notes-off. This matters now the engine is
// polyphonic — a note-off that did not say which note could only ever mean "stop everything".
void sound_engine_note(int32_t note, bool on);

// Whether the engine will sound more than one note at once, i.e. the patch is Poly with a voice
// count above 1. The note stack needs this: in Mono, releasing a key falls back to the newest note
// still held, and in Poly it must not, because that note already has a voice of its own sounding it.
bool sound_engine_is_polyphonic(void);

// How many voices the current patch may sound at once, and how many are audible right now. For the
// status line — the second figure is what tells you whether a chord is being cut short.
uint32_t sound_engine_voice_count(void);
uint32_t sound_engine_voices_sounding(void);

// Worst render load since this was last called, as a percentage of real time — reading it clears the
// peak. Approaching 100 % means the engine is running out of its deadline, which is what crackling
// is; well below it means a crackle is something else.
uint32_t sound_engine_load_percent(void);

// UI thread. Reads the current selection and publishes a parameter snapshot for the audio thread.
// Cheap enough to call on every redraw, which is what graphics.c does — every parameter change
// forces one, so nothing else needs to poll.
void sound_engine_update_from_patch(void);

// The resolved chain as the engine currently sees it — one line per node with the parameters it
// actually read. For diagnosing "it looks right on screen but makes no sound": the usual causes are
// a parameter read from the wrong variation, or a chain that resolved differently than it looks.
// UI thread only.
const char * sound_engine_debug_text(void);

// Audio thread, real-time context: no locks, no allocation, no logging below this line.
// Fills frameCount frames of interleaved float, channelCount channels wide.
void sound_engine_set_sample_rate(double sampleRate);
void sound_engine_render(float * out, uint32_t frameCount, uint32_t channelCount);

#ifdef __cplusplus
}
#endif

#endif // __SOUND_ENGINE_H__
