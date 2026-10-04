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

// Windows version of src/misc.mm's five functions (Docs/windows-port-plan.md).

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "misc.h"
#include "prefs.h"
#include "audioOutput.h"
#include "midiInput.h"

// No native menu bar to make (the application draws its own); the rest is misc.mm's, unchanged.
void setup_main_menu(void) {
    prefs_init("G2-Edit");
    audio_output_load_settings();
    midi_input_load_settings();
    load_saved_settings();
}

// Windows does not nap a foreground process the way macOS does; the audio thread's own priority is
// the WASAPI version's business.
void platform_begin_audio_activity(void) {
}

void platform_end_audio_activity(void) {
}

// TODO: WM_POWERBROADCAST / PBT_APMRESUMEAUTOMATIC -> usb_signal_reconnect(), once USB works here.
void register_sleep_wake_notifications(void) {
}

bool platform_any_mouse_button_down(void) {
    return ((GetAsyncKeyState(VK_LBUTTON) | GetAsyncKeyState(VK_RBUTTON) | GetAsyncKeyState(VK_MBUTTON)) & 0x8000) != 0;
}
