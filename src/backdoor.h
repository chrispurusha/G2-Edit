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

#ifndef __BACKDOOR_H__
#define __BACKDOOR_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif

// The backdoor test-control channel — a file-driven way to drive AND independently verify the
// running app. The command surface, the gating environment variable and the reason each command
// exists are all documented at the top of backdoor.c.
//
// Only two entry points, both called from the render loop (graphics.c): backdoor_poll() honours one
// command per tick, and backdoor_enabled() is what tells that loop to tick at all — an unset
// G2_EDIT_BACKDOOR leaves the channel completely inert and the idle loop asleep in glfwWaitEvents().
bool backdoor_enabled(void);
void backdoor_poll(void);

#ifdef __cplusplus
}
#endif

#endif // __BACKDOOR_H__
