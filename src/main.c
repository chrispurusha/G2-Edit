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

#ifdef __cplusplus
extern "C" {
#endif

#include <signal.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "graphics.h"
#include "dataBase.h"
#include "usbComms.h"
#include "misc.h"
#include "globalVars.h"
#include "prefs.h"
#include "moduleResourcesAccess.h"
#include "mouseHandle.h"
#include "soundEngine.h"
#include "midiInput.h"
#include "main.h"

static void signal_handler(int sigraised) {
    LOG_DEBUG("\nSig Handler!!! %d\n", sigraised);

    synthlib_request_quit();

    _exit(0);
}

static int init_signals(void) {
    int retVal = EXIT_FAILURE;

    if (signal(SIGINT, signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }

    if (signal(SIGBUS, signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }

    if (signal(SIGSEGV, signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }

    if (signal(SIGTERM, signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }

    if (signal(SIGABRT, signal_handler) != SIG_ERR) {
        retVal = EXIT_SUCCESS;
    }
    return retVal;
}

int main(int argc, char ** argv) {
    init_signals();

    init_database();
    init_module_resource_cache();

    // Give every slot a default patch (including an activated, source-assigned morph module — see
    // init_patch()) before the first frame renders, rather than leaving the database's zeroed/
    // inactive startup state on screen until a G2 connects and sends real patch data. If a
    // connection succeeds shortly after, send_init_sequence_pull()'s real data simply overwrites
    // this placeholder per slot, same as loading over a manually-created New Patch would.
    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        init_patch(slot);
    }

    // BEFORE init_graphics(), and the order is load-bearing. The window is built differently for
    // each render backend — OpenGL needs a GL context created alongside it, Metal needs none — so
    // synthlib_window_create() reads the saved choice before it makes the window. prefs_init() also
    // runs from setup_main_menu() below, where it always did; it clears and re-reads, so calling it
    // twice is harmless and nothing has written a preference in between.
    prefs_init("G2-Edit");

    init_graphics();

    register_sleep_wake_notifications();
    setup_main_menu();

    // MIDI input runs for the life of the application rather than with the sound engine: incoming
    // notes can be sent on to the G2, which is worth having whether or not the engine is switched on.
    if (midi_input_start() == false) {
        LOG_ERROR("MIDI input unavailable\n");
    }
    start_usb_thread();

    do_graphics_loop();

    // Before the graphics teardown: stopping the engine waits for any render in flight to return,
    // and that render reads patch data the rest of the shutdown is entitled to tear down.
    sound_engine_stop();
    midi_input_stop();

    clean_up_graphics();

    exit(EXIT_SUCCESS);
}

#ifdef __cplusplus
}
#endif
