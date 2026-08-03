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

#ifndef __AUDIO_OUTPUT_H__
#define __AUDIO_OUTPUT_H__

#include "sysIncludes.h"

// The platform half of the sound engine: opens the system's default output device and pulls audio
// from soundEngine.c on the real-time thread CoreAudio provides. This is the only file in the
// engine that knows what operating system it is on — soundEngine.c is plain C and stays portable.
//
// Nothing here is called directly from the UI. sound_engine_start()/stop() own the lifetime; go
// through those.

bool audio_output_start(void);
void audio_output_stop(void);

// The device's sample rate once running, or 0 if it is not. Informational — the engine is told the
// rate directly when the device opens.
double audio_output_sample_rate(void);

#endif // __AUDIO_OUTPUT_H__
