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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "defs.h"
#include "usbLog.h"

#ifdef ENABLE_USB_LOG

static FILE * logFile = NULL;

void usb_log_open(void) {
    char         path[1024] = {0};
    const char * home       = getenv("HOME");

    if (home != NULL) {
        snprintf(path, sizeof(path), "%s/G2_usb.log", home);
    } else {
        snprintf(path, sizeof(path), "/tmp/G2_usb.log");
    }
    logFile = fopen(path, "w");

    if (logFile == NULL) {
        fprintf(stderr, "usb_log_open: failed to open %s\n", path);
        return;
    }
    fprintf(logFile, "--- session start ---\n");
    fflush(logFile);
}

void usb_log_close(void) {
    if (logFile != NULL) {
        fprintf(logFile, "--- session end ---\n");
        fflush(logFile);
        fclose(logFile);
        logFile = NULL;
    }
}

void usb_log_text(const char * fmt, ...) {
    struct timespec ts;
    struct tm       tmInfo;
    va_list         args;

    if (logFile == NULL) {
        return;
    }
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tmInfo);

    fprintf(logFile, "[%02d:%02d:%02d.%03d] ",
            tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec,
            (int)(ts.tv_nsec / 1000000));

    va_start(args, fmt);
    vfprintf(logFile, fmt, args);
    va_end(args);

    fflush(logFile);
}

void usb_log_message(const char * direction, const uint8_t * data, size_t length) {
    struct timespec ts;
    struct tm       tmInfo;
    size_t          i;

    if (logFile == NULL) {
        return;
    }
    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tmInfo);

    fprintf(logFile, "[%02d:%02d:%02d.%03d] %s %zu bytes:",
            tmInfo.tm_hour, tmInfo.tm_min, tmInfo.tm_sec,
            (int)(ts.tv_nsec / 1000000),
            direction, length);

    for (i = 0; i < length; i++) {
        fprintf(logFile, " %02x", data[i]);
    }

    fprintf(logFile, "\n");
    fflush(logFile);
}

#endif // ENABLE_USB_LOG
