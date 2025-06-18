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

#ifndef __USB_COMMS_H__
#define __USB_COMMS_H__

#include "sysIncludes.h"

void start_usb_thread(void);
void register_glfw_wake_cb(void ( *func_ptr )(void));
void register_full_patch_change_notify_cb(void ( *func_ptr )(void));
int parse_patch(uint32_t slot, uint8_t * buff, int length); // TODO: also accessed from file, so need to decide how to access from USB and file

#endif // __USB_COMMS_H__
