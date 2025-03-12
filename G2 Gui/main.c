/*
 * The G2 Editor application.
 *
 * Copyright (C) 2025 Chris Turner <chris_purusha@icloud.com>
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

#include "graphics.h"
#include "usbComms.h"
#include "main.h"

static void signal_handler(int sigraised) {
    // ToDo - deal with signals properly
    printf("\nSig Handler!!! %d\n", sigraised);

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
    return retVal;
}

int main(int argc, char ** argv) {
    usleep(2000000);     // Purely required by XCode debugging and can be removed for production

    init_signals();

    init_graphics();

    start_usb_thread();

    do_graphics_loop();

    clean_up_graphics();

    exit(EXIT_SUCCESS);
}

#ifdef __cplusplus
}
#endif