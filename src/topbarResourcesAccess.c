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

#ifdef __cplusplus
extern "C" {
#endif

#include "topbarResources.h"
#include "topbarResourcesAccess.h"
#include "globalVars.h"

const tTopbarControlDef * topbar_control_def(tTopbarControlId id) {
    return &topbarControlList[id];
}

void topbar_init_controls(void) {
    int i = 0;

    for (i = 0; i < topbarControlMax; i++) {
        gTopbarControls[i].colour    = topbarControlList[i].defaultColour;
        gTopbarControls[i].isPressed = false;
    }

    gTopbarControls[topbarPatchVolumeId].rectangle = (tRectangle){{
                                                                      320.0, 56.0
                                                                  }, {
                                                                      20.0, 20.0
                                                                  }
    };
}

#ifdef __cplusplus
}
#endif
