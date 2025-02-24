/*
 * The G2 Editor application.
 *
 * Copyright (C) 2024 Chris Turner <chris_purusha@icloud.com>
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


#ifndef __IOKIT_H__
#define __IOKIT_H__

#include "sysIncludes.h"

#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>

#include <mach/mach.h>

IOUSBDeviceInterface ** find_usb_device(void);
void release_usb_device(IOUSBDeviceInterface ** deviceInterface);
void reset_usb(void);
int open_usb(void);
void close_usb(void);
int32_t write_usb(uint8_t * buff, uint32_t length);
int32_t read_usb_extended(uint8_t * buff, uint32_t buffLength);
void read_usb_complete(void * refCon, IOReturn result, void * arg0);
void timeout_callback(CFRunLoopTimerRef timer, void * info);
int32_t read_usb_interrupt(uint8_t * buff, uint32_t buffLength);

#endif // __IOKIT_H__