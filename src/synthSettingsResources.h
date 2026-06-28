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

// Include only in synthSettingsResources.c, graphics.cpp, and mousePanels.c.

#ifndef SYNTH_SETTINGS_RESOURCES_H
#define SYNTH_SETTINGS_RESOURCES_H

#include "types.h"

// One entry per control in the Synth Settings panel.
// get_str returns the current value string; get_colour the button background;
// act is called with below_rect(*rect) on click.
typedef struct {
    const char * label;
    tRectangle * rect;
    const char * widest;
    const char * (*get_str)(void);
    tRgb (*get_colour)(void);
    void (*act)(tCoord);
} tSynthSettingItem;

#define kSSGlobalCount     2
#define kSSOptionsCount    8
#define kSSTuningCount     4
#define kSSPedalCount      2
#define kSSSortCount       2

extern const tSynthSettingItem kSSGlobal[kSSGlobalCount];
extern const tSynthSettingItem kSSOptions[kSSOptionsCount];
extern const tSynthSettingItem kSSTuning[kSSTuningCount];
extern const tSynthSettingItem kSSPedal[kSSPedalCount];
extern const tSynthSettingItem kSSSort[kSSSortCount];

#endif /* SYNTH_SETTINGS_RESOURCES_H */
