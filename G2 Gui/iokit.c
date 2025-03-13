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

#include "iokit.h"

#define G2_VENDOR_ID     (0xffc)
#define G2_PRODUCT_ID    (2)

static IOUSBInterfaceInterface ** intf     = NULL;
static bool                       timedOut = false;

IOUSBDeviceInterface ** find_usb_device(void) {
    mach_port_t             masterPort      = 0;
    io_service_t            usbDevice       = {0};
    io_iterator_t           iterator        = {0};
    IOCFPlugInInterface **  pluginInterface = NULL;
    SInt32                  score           = 0;
    UInt16                  vendor          = 0;
    UInt16                  product         = 0;
    IOUSBDeviceInterface ** deviceInterface = NULL;
    bool                    quitLoop        = false;

#if (MAC_OS_X_VERSION_MIN_REQUIRED >= 120000)
    if (IOMainPort(MACH_PORT_NULL, &masterPort) == kIOReturnSuccess)
#else
    if (IOMasterPort(MACH_PORT_NULL, &masterPort) == kIOReturnSuccess)
#endif
    {
        if (masterPort != 0) {
            IORegistryCreateIterator(masterPort, kIOUSBPlane, kIORegistryIterateRecursively, &iterator);

            do {
                usbDevice = IOIteratorNext(iterator);

                if (usbDevice != 0) {
                    IOCreatePlugInInterfaceForService(usbDevice, kIOUSBDeviceUserClientTypeID, kIOCFPlugInInterfaceID, &pluginInterface, &score);
                    IOObjectRelease(usbDevice);

                    if ((pluginInterface != NULL) && (*pluginInterface != NULL)) {
                        (*pluginInterface)->QueryInterface(pluginInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID245), (LPVOID)&deviceInterface);
                        IODestroyPlugInInterface(pluginInterface);

                        if (*deviceInterface != NULL) {
                            (*deviceInterface)->GetDeviceVendor(deviceInterface, &vendor);
                            (*deviceInterface)->GetDeviceProduct(deviceInterface, &product);

                            if ((vendor == G2_VENDOR_ID) && (product == G2_PRODUCT_ID)) {
                                //printf("found device (vendor = 0x%x, product = %d)\n", vendor, product);
                                quitLoop = true;
                            } else {
                                deviceInterface = NULL;
                            }
                        }
                    }
                } else {
                    quitLoop = true;
                }
            } while (quitLoop == false);
        }
    }

    if (masterPort != 0) {
        mach_port_deallocate(mach_task_self(), masterPort);
        masterPort = 0;
    }
    return deviceInterface;
}

void release_usb_device(IOUSBDeviceInterface ** deviceInterface) {
    if (deviceInterface != NULL) {
        (*deviceInterface)->USBDeviceClose(deviceInterface);
        (*deviceInterface)->Release(deviceInterface);
    }
}

void reset_usb(void) {
    IOUSBDeviceInterface ** deviceInterface = NULL;

    deviceInterface = find_usb_device();

    if (deviceInterface != NULL) {
        (*deviceInterface)->USBDeviceOpenSeize(deviceInterface);
        (*deviceInterface)->USBDeviceReEnumerate(deviceInterface, 0);         // Instead of ResetDevice
        (*deviceInterface)->USBDeviceClose(deviceInterface);

        release_usb_device(deviceInterface);
    }
}

int open_usb(void) {
    int                       retVal             = EXIT_FAILURE;
    SInt32                    score              = 0;
    IOUSBFindInterfaceRequest request            = {0};
    IOCFPlugInInterface **    plugInInterface    = NULL;
    io_iterator_t             iterator2          = 0;
    io_service_t              usbInterface       = 0;
    IOUSBDeviceInterface **   deviceInterface    = NULL;
    CFRunLoopSourceRef        asyncRunLoopSource = NULL;

    deviceInterface = find_usb_device();

    if (deviceInterface != NULL) {
        request.bInterfaceClass    = kIOUSBFindInterfaceDontCare;
        request.bInterfaceSubClass = kIOUSBFindInterfaceDontCare;
        request.bInterfaceProtocol = kIOUSBFindInterfaceDontCare;
        request.bAlternateSetting  = kIOUSBFindInterfaceDontCare;

        (*deviceInterface)->CreateInterfaceIterator(deviceInterface, &request, &iterator2);

        usbInterface = IOIteratorNext(iterator2);

        IOCreatePlugInInterfaceForService(usbInterface, kIOUSBInterfaceUserClientTypeID, kIOCFPlugInInterfaceID, &plugInInterface, &score);
        IOObjectRelease(usbInterface);

        if (plugInInterface != NULL) {
            (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID)&intf);
            IODestroyPlugInInterface(plugInInterface);

            (*intf)->USBInterfaceOpen(intf);

            (*intf)->CreateInterfaceAsyncEventSource(intf, &asyncRunLoopSource);

            if (asyncRunLoopSource) {
                CFRunLoopAddSource(CFRunLoopGetCurrent(), asyncRunLoopSource, kCFRunLoopDefaultMode);
            }
            release_usb_device(deviceInterface);

            retVal = EXIT_SUCCESS;
        }
    }
    return retVal;
}

void close_usb(void) {
    if (intf != NULL) {
        (*intf)->USBInterfaceClose(intf);
        (*intf)->Release(intf);
        intf = NULL;
    }
}

int32_t write_usb(uint8_t * buff, uint32_t length) {
    uint32_t writeLength = 0;

    if (buff == NULL) {
        return writeLength;
    }

    if ((*intf)->WritePipeTO(intf, 3, (void *)buff, length, 1000, 1000) == kIOReturnSuccess) {
        writeLength = length;
    }
    return writeLength;
}

int32_t read_usb_extended(uint8_t * buff, uint32_t buffLength) {
    uint32_t readLength = 0;

    if (buff == NULL) {
        return readLength;
    }
    memset(buff, 0, buffLength);
    readLength = buffLength;

    if ((*intf)->ReadPipeTO(intf, 2, (void *)buff, &readLength, 1000, 1000) != kIOReturnSuccess) {
        readLength = 0;
    }
    return readLength;
}

void read_usb_complete(void * refCon, IOReturn result, void * arg0) {
    CFRunLoopStop(CFRunLoopGetCurrent());
}

void timeout_callback(CFRunLoopTimerRef timer, void * info) {
    timedOut = true;

    if ((*intf)->AbortPipe(intf, 1) != kIOReturnSuccess) {
        CFRunLoopStop(CFRunLoopGetCurrent());
    }
}

int32_t read_usb_interrupt(uint8_t * buff, uint32_t buffLength) {
    uint32_t          readLength = 0;
    CFTimeInterval    interval   = 0.02;  // 20ms
    CFRunLoopTimerRef timer      = NULL;
    IOReturn          result     = kIOReturnError;

    if (buff == NULL) {
        return readLength;
    }
    memset(buff, 0, buffLength);
    timedOut = false;

    timer = CFRunLoopTimerCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() + interval, 0, 0, 0, timeout_callback, NULL);
    CFRunLoopAddTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);

    result = (*intf)->ReadPipeAsync(intf, 1, (void *)buff, buffLength, read_usb_complete, NULL);

    CFRunLoopRun();

    CFRunLoopRemoveTimer(CFRunLoopGetCurrent(), timer, kCFRunLoopDefaultMode);
    CFRelease(timer);

    if (timedOut == true) {
        readLength = 0;
    } else if (result == kIOReturnSuccess) {   // Unless there's a way to detect aborted pipe read
        if ((buff[0] != 0)) {
            readLength = buffLength;
        }
    } else if (result == kIOReturnNoDevice) {
        readLength = -1;
    }
    return readLength;
}

#ifdef __cplusplus
}
#endif
