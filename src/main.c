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
#include "moduleResourcesAccess.h"
#include "mouseHandle.h"
#include "main.h"

static void signal_handler(int sigraised) {
    LOG_DEBUG("\nSig Handler!!! %d\n", sigraised);

    gQuitAll = true;

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

    init_graphics();

    register_sleep_wake_notifications();
    setup_main_menu();

    start_usb_thread();

    do_graphics_loop();

    clean_up_graphics();

    exit(EXIT_SUCCESS);
}

#ifdef __cplusplus
}
#endif
