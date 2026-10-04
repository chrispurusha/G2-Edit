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

// Force-included into every Windows compile by cmake/platform.cmake: the small POSIX and OpenGL
// gaps MinGW leaves, filled here so the shared sources need no #ifdefs for them.

#ifndef __WIN_COMPAT_H__
#define __WIN_COMPAT_H__

#include <signal.h>
#include <io.h>
#include <direct.h>

// No bus errors on Windows; a handler installed for one is installed for the segfault instead.
#ifndef SIGBUS
#define SIGBUS    SIGSEGV
#endif

// MinGW's mkdir() takes no mode; Windows has no permission bits to give it.
#define mkdir(path, mode)    mkdir(path)

// Windows' gl.h stops at OpenGL 1.1; the driver takes these, the header just does not name them.
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE      0x809D
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE    0x812F
#endif

#endif // __WIN_COMPAT_H__
