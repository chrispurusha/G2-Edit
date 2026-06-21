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

#ifndef __USB_LOG_H__
#define __USB_LOG_H__

#include <stdint.h>
#include <stddef.h>
#include "defs.h"

#ifdef ENABLE_USB_LOG
void usb_log_open(void);
void usb_log_close(void);
void usb_log_message(const char * direction, const uint8_t * data, size_t length);
void usb_log_text(const char * fmt, ...);
#else
#define usb_log_open()
#define usb_log_close()
#define usb_log_message(direction, data, length)    ((void)0)
#define usb_log_text(fmt, ...)                      ((void)0)
#endif

#endif // __USB_LOG_H__
