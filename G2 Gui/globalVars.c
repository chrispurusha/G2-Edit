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

GLFWwindow *    gWindow            = NULL;
uint32_t        gLocation          = locationVa;
uint32_t        gVariation         = 0;
bool            gReDraw            = true;
tButton         gMainButtonArray[] = {  // Must align with tButtonId enumaration
    {{400,  8}, NULL_RECTANGLE, anchorTopLeft, "VA",        RGB_GREEN_ON       },
    {{425,  8}, NULL_RECTANGLE, anchorTopLeft, "FX",        RGB_BACKGROUND_GREY},
    {{ 20,  8}, NULL_RECTANGLE, anchorTopLeft, "Read File", RGB_BACKGROUND_GREY},
    {{400, 60}, NULL_RECTANGLE, anchorTopLeft, "1",         RGB_GREEN_ON       },
    {{412, 60}, NULL_RECTANGLE, anchorTopLeft, "2",         RGB_BACKGROUND_GREY},
    {{424, 60}, NULL_RECTANGLE, anchorTopLeft, "3",         RGB_BACKGROUND_GREY},
    {{436, 60}, NULL_RECTANGLE, anchorTopLeft, "4",         RGB_BACKGROUND_GREY},
    {{448, 60}, NULL_RECTANGLE, anchorTopLeft, "5",         RGB_BACKGROUND_GREY},
    {{460, 60}, NULL_RECTANGLE, anchorTopLeft, "6",         RGB_BACKGROUND_GREY},
    {{472, 60}, NULL_RECTANGLE, anchorTopLeft, "7",         RGB_BACKGROUND_GREY},
    {{484, 60}, NULL_RECTANGLE, anchorTopLeft, "8",         RGB_BACKGROUND_GREY},
    {{500, 60}, NULL_RECTANGLE, anchorTopLeft, "Init",      RGB_BACKGROUND_GREY},
    {{600, 8}, NULL_RECTANGLE,  anchorTopLeft, "A",         RGB_GREEN_ON},
    {{615, 8}, NULL_RECTANGLE,  anchorTopLeft, "B",         RGB_BACKGROUND_GREY},
    {{630, 8}, NULL_RECTANGLE,  anchorTopLeft, "C",         RGB_BACKGROUND_GREY},
    {{645, 8}, NULL_RECTANGLE,  anchorTopLeft, "D",         RGB_BACKGROUND_GREY},
};

bool            gShowOpenFileReadDialogue = false;

tScrollState    gScrollState     = {(SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, NULL_RECTANGLE, (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, NULL_RECTANGLE};
tContextMenu    gContextMenu     = {0};
tCableDragging  gCableDrag       = {0};
tParamDragging  gParamDragging   = {0};
tModuleDragging gModuleDrag      = {0};
tMessageQueue   gCommandQueue    = {0};
uint32_t        gMorphGroupFocus = 0;
uint32_t        gSlot = 0;

uint32_t array_size_main_button_array(void) {
    return ARRAY_SIZE(gMainButtonArray);
}

#ifdef __cplusplus
}
#endif
