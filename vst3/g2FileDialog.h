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

#ifndef __G2_FILE_DIALOG_H__
#define __G2_FILE_DIALOG_H__

#include "sysIncludes.h"

#ifdef __cplusplus
extern "C" {
#endif

// Puts up the system open panel and returns the chosen .pch2 path. False if the user cancelled or
// nothing usable came back. Main thread only — it runs a modal panel.
bool g2_choose_patch_file(char * pathOut, size_t pathLen);

#ifdef __cplusplus
}
#endif

#endif // __G2_FILE_DIALOG_H__
