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

#define TOPBAR_COL(r, g, b)    {r, g, b}
#define TOPBAR_COL_BG_GREY     TOPBAR_COL(0.8, 0.8, 0.8)
#define TOPBAR_COL_GREEN_ON    TOPBAR_COL(0.3, 0.7, 0.3)
#define TOPBAR_COL_BLACK       TOPBAR_COL(0.0, 0.0, 0.0)

#define X(id, x, y, label, col, type) \
   {id, {x, y}, anchorTopLeft, label, col, type},

static const tTopbarControlDef topbarControlList[] = {
#include "topbarControls.def"
};

#undef X
#undef TOPBAR_COL_BLACK
#undef TOPBAR_COL_GREEN_ON
#undef TOPBAR_COL_BG_GREY
#undef TOPBAR_COL

#endif // __TOPBAR_RESOURCES_H__
