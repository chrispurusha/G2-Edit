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

// Only include from topbarResourcesAccess.c

#ifndef __TOPBAR_RESOURCES_H__
#define __TOPBAR_RESOURCES_H__

#include "types.h"
#include "defs.h"

static const tTopbarControlDef topbarControlList[topbarControlMax] = {
    // Standard buttons — static text, generic rendering (IDs 0..topbarVariationInitId)
    [topbarVaId]                 = {topbarVaId,                 {400,  8}, anchorTopLeft, "VA", RGB_GREEN_ON,        topbarControlTypeButton },
    [topbarFxId]                 = {topbarFxId,                 {425,  8}, anchorTopLeft, "FX", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarOpenReadFileId]       = {topbarOpenReadFileId,       { 20,  8}, anchorTopLeft, "Read File", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarOpenWriteFileId]      = {topbarOpenWriteFileId,      { 80,  8}, anchorTopLeft, "Write File", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarNewPatchId]           = {topbarNewPatchId,           {140,  8}, anchorTopLeft, "New Patch", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarSlotAId]              = {topbarSlotAId,              {600,  8}, anchorTopLeft, "A", RGB_GREEN_ON,        topbarControlTypeButton },
    [topbarSlotBId]              = {topbarSlotBId,              {615,  8}, anchorTopLeft, "B", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarSlotCId]              = {topbarSlotCId,              {630,  8}, anchorTopLeft, "C", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarSlotDId]              = {topbarSlotDId,              {645,  8}, anchorTopLeft, "D", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarVariation1Id]         = {topbarVariation1Id,         {400, 60}, anchorTopLeft, "1", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarVariation2Id]         = {topbarVariation2Id,         {412, 60}, anchorTopLeft, "2", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarVariation3Id]         = {topbarVariation3Id,         {424, 60}, anchorTopLeft, "3", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarVariation4Id]         = {topbarVariation4Id,         {436, 60}, anchorTopLeft, "4", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarVariation5Id]         = {topbarVariation5Id,         {448, 60}, anchorTopLeft, "5", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarVariation6Id]         = {topbarVariation6Id,         {460, 60}, anchorTopLeft, "6", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarVariation7Id]         = {topbarVariation7Id,         {472, 60}, anchorTopLeft, "7", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarVariation8Id]         = {topbarVariation8Id,         {484, 60}, anchorTopLeft, "8", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    [topbarVariationInitId]      = {topbarVariationInitId,      {500, 60}, anchorTopLeft, "Init", RGB_BACKGROUND_GREY, topbarControlTypeButton },
    // Special controls — dynamic text or custom rendering
    [topbarPatchNameId]          = {topbarPatchNameId,          { 20, 60}, anchorTopLeft, NULL, RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarPatchTypeId]          = {topbarPatchTypeId,          {170, 60}, anchorTopLeft, NULL, RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarMonoPolyId]           = {topbarMonoPolyId,           {270, 60}, anchorTopLeft, NULL, RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarVoiceCountId]         = {topbarVoiceCountId,         {248, 60}, anchorTopLeft, NULL, RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarPatchNotesId]         = {topbarPatchNotesId,         {340,  8}, anchorTopLeft, "Notes", RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarSettingsId]           = {topbarSettingsId,           {270,  8}, anchorTopLeft, "Settings", RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarClockRunStopId]       = {topbarClockRunStopId,       {475,  8}, anchorTopLeft, NULL, RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarTempoDialId]          = {topbarTempoDialId,          {535, 10}, anchorTopLeft, NULL, RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarPatchVolumeId]        = {topbarPatchVolumeId,        {320, 56}, anchorTopLeft, NULL, RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarPerfNameId]           = {topbarPerfNameId,           {120, 42}, anchorTopLeft, NULL, RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarHideAllCablesId]      = {topbarHideAllCablesId,      {700, 40}, anchorTopLeft, "Hide", RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    [topbarTransparentCablesId]  = {topbarTransparentCablesId,  {740, 40}, anchorTopLeft, "Dim", RGB_BACKGROUND_GREY, topbarControlTypeSpecial},
    // Cable colour toggles — x position computed at render time
    [topbarCableColourToggle0Id] = {topbarCableColourToggle0Id, {  0, 60}, anchorTopLeft, NULL,         {0,                                      0, 0},topbarControlTypeSpecial },
    [topbarCableColourToggle1Id] = {topbarCableColourToggle1Id, {  0, 60}, anchorTopLeft, NULL,         {0,                                      0, 0},topbarControlTypeSpecial },
    [topbarCableColourToggle2Id] = {topbarCableColourToggle2Id, {  0, 60}, anchorTopLeft, NULL,         {0,                                      0, 0},topbarControlTypeSpecial },
    [topbarCableColourToggle3Id] = {topbarCableColourToggle3Id, {  0, 60}, anchorTopLeft, NULL,         {0,                                      0, 0},topbarControlTypeSpecial },
    [topbarCableColourToggle4Id] = {topbarCableColourToggle4Id, {  0, 60}, anchorTopLeft, NULL,         {0,                                      0, 0},topbarControlTypeSpecial },
    [topbarCableColourToggle5Id] = {topbarCableColourToggle5Id, {  0, 60}, anchorTopLeft, NULL,         {0,                                      0, 0},topbarControlTypeSpecial },
    [topbarCableColourToggle6Id] = {topbarCableColourToggle6Id, {  0, 60}, anchorTopLeft, NULL,         {0,                                      0, 0},topbarControlTypeSpecial }, };

#endif // __TOPBAR_RESOURCES_H__
