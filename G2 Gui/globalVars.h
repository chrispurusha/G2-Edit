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

#ifndef __GLOBAL_VARS_H__
#define __GLOBAL_VARS_H__

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include "sysIncludes.h"
#include "defs.h"
#include "types.h"
#include "msgQueue.h"

extern GLFWwindow *    gWindow;
extern uint32_t        gLocation;
extern uint32_t        gVariation;
extern bool            gReDraw;
extern tButton         gSelectVa;
extern tButton         gSelectFx;
extern tButton         gSelectOpenReadFile;
extern bool            gShowOpenFileReadDialogue;
extern tScrollState    gScrollState;
extern tContextMenu    gContextMenu;
extern tCableDragging  gCableDrag;
extern tDialDragging   gDialDragging;
extern tModuleDragging gModuleDrag;
extern tMessageQueue   gCommandQueue;

#endif // __GLOBAL_VARS_H__
