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

// Include only in patchParamsResources.c and graphics.cpp.

#ifndef PATCH_PARAMS_RESOURCES_H
#define PATCH_PARAMS_RESOURCES_H

#include "types.h"

typedef struct {
    const char *      label;
    tPatchParamRectId rectId;
    const char *      widest;
    const char * (*get_str)(void);
    tRgb (*get_colour)(void);
} tPatchParamItem;

#define kPPArpCount        4
#define kPPVibratoCount    2
#define kPPGlideCount      2
#define kPPBendCount       2

extern const tPatchParamItem kPPArp[kPPArpCount];
extern const tPatchParamItem kPPVibrato[kPPVibratoCount];
extern const tPatchParamItem kPPGlide[kPPGlideCount];
extern const tPatchParamItem kPPBend[kPPBendCount];

#endif /* PATCH_PARAMS_RESOURCES_H */
