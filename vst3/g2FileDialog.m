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

// Choosing a patch file, in a host's process.
//
// A deliberately tiny NSOpenPanel rather than reusing misc.mm's file_menu_open_patch(). That one
// loads the file as well as choosing it, and it does so through graphics.c's loader, which carries
// the online branch and pulls in GLFW. All the plug-in wants is a PATH; it already has its own
// loader in g2Patch.c.
//
// Plain Objective-C: NSOpenPanel needs the runtime, nothing here needs C++.
//
// A SANDBOXED HOST may deny access to the chosen file — but a file the USER picked through the
// system panel carries its own consent, which is the whole reason a panel is used rather than a
// path typed into a text field. This is also why the plug-in still ships with a patch compiled in:
// see do-vst3.

#import <Cocoa/Cocoa.h>

#include "g2FileDialog.h"

bool g2_choose_patch_file(char * pathOut, size_t pathLen) {
    if ((pathOut == NULL) || (pathLen == 0)) {
        return false;
    }
    pathOut[0] = '\0';

    @autoreleasepool {
        NSOpenPanel * panel = [NSOpenPanel openPanel];

        [panel setAllowsMultipleSelection:NO];
        [panel setCanChooseDirectories:NO];
        [panel setCanChooseFiles:YES];
        [panel setTitle:@"Open Patch"];
        [panel setAllowedFileTypes:@[@"pch2"]];

        // runModal, not a sheet. A sheet must be attached to a window we would have to borrow from
        // the host, and hosts differ about what they will lend; a modal panel needs nobody's window.
        if ([panel runModal] != NSModalResponseOK) {
            return false;
        }
        NSURL * url = [[panel URLs] firstObject];

        if (url == nil) {
            return false;
        }
        const char * path = [[url path] fileSystemRepresentation];

        if (path == NULL) {
            return false;
        }
        strncpy(pathOut, path, pathLen - 1);
        pathOut[pathLen - 1] = '\0';
    }
    return true;
}
