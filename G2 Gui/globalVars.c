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

#include "globalVars.h"

// Todo: find a better way/place to define these functions
extern void va_button(void);
extern void fx_button(void);
extern void open_read_file_button(void);

GLFWwindow *    gWindow                   = NULL;
uint32_t        gLocation                 = locationVa;
bool            gReDraw                   = true;
tButton         gSelectVa                 = {NULL_RECTANGLE, anchorTopLeft, "VA", va_button};        // TODO: put these select items in an array of structures
tButton         gSelectFx                 = {NULL_RECTANGLE, anchorTopLeft, "FX", fx_button};
tButton         gSelectOpenReadFile       = {NULL_RECTANGLE, anchorTopLeft, "Read File", open_read_file_button};
bool            gShowOpenFileReadDialogue = false;
tScrollState    gScrollState              = {(SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false};
tContextMenu    gContextMenu              = {0};
tCableDragging  gCableDrag                = {0};
tDialDragging   gDialDragging             = {0};
tModuleDragging gModuleDrag               = {0};
tMessageQueue   gCommandQueue             = {0};

#ifdef __cplusplus
}
#endif
