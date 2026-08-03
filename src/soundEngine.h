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

// Whether the engine is running and holding the audio device.
bool sound_engine_active(void);

// One line saying what the engine is doing, or why it is silent — "Playing OscB", "Select an OscB
// to play", and so on. The Experimental menu shows this under the toggle, because an engine that is
// on but silent otherwise gives no clue which of the several reasons applies. UI thread only.
const char * sound_engine_status_text(void);

// Opens the audio device and starts rendering. False if the device would not start, in which case
// nothing is held and the engine stays inactive.
bool sound_engine_start(void);
void sound_engine_stop(void);

// Note input. Called from the UI thread — see set_sounding_note() in virtualKeyboard.c, which is
// the single point every note change goes through. Passing on == false with any note silences.
void sound_engine_note(int32_t note, bool on);

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

#endif // __SOUND_ENGINE_H__
