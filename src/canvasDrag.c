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

// See canvasDrag.h. Lifted from cursor_pos() in mouseHandle.c, unchanged in behaviour — the only
// difference is that the coordinate arrives as an argument instead of being read from GLFW.

#include "sysIncludes.h"
#include "synthlibDefs.h"
#include "types.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "defs.h"
#include "globalVars.h"
#include "dataBase.h"
#include "selection.h"
#include "canvasCoords.h"
#include "canvasDrag.h"

bool canvas_drag_motion(tCoord coord) {
    if (gModuleDrag.active == true) {
        if (gModuleDrag.isMulti) {
            uint32_t newCol = 0;
            uint32_t newRow = 0;

            convert_mouse_coord_to_module_column_row(&newCol, &newRow, coord);

            if (newCol > MAX_COLUMNS) {
                newCol = MAX_COLUMNS;
            }

            if (newRow > MAX_ROWS) {
                newRow = MAX_ROWS;
            }
            int32_t  dc     = (int32_t)newCol - (int32_t)gModuleDrag.prevColumn;
            int32_t  dr     = (int32_t)newRow - (int32_t)gModuleDrag.prevRow;

            // Everything selected moves by the SAME delta, rather than each module jumping to the
            // pointer — otherwise a multiple selection collapses onto one square as soon as it moves.
            if ((dc != 0) || (dr != 0)) {
                for (uint32_t i = 0; i < gSelection.count; i++) {
                    tModule * sel = get_module(gSelection.keys[i]);

                    if (sel == NULL) {
                        continue;
                    }
                    int32_t   nc  = (int32_t)sel->column + dc;
                    int32_t   nr  = (int32_t)sel->row + dr;

                    if (nc < 0) {
                        nc = 0;
                    }

                    if (nr < 0) {
                        nr = 0;
                    }

                    if (nc > (int32_t)MAX_COLUMNS) {
                        nc = (int32_t)MAX_COLUMNS;
                    }

                    if (nr > (int32_t)MAX_ROWS) {
                        nr = (int32_t)MAX_ROWS;
                    }
                    sel->column = (uint32_t)nc;
                    sel->row    = (uint32_t)nr;
                }

                gModuleDrag.prevColumn = newCol;
                gModuleDrag.prevRow    = newRow;
            }
        } else {
            tModule * module = get_module(gModuleDrag.moduleKey);

            if (module != NULL) {
                convert_mouse_coord_to_module_column_row(&module->column, &module->row, coord);

                if (module->row > MAX_ROWS) {
                    module->row = MAX_ROWS;
                }

                if (module->column > MAX_COLUMNS) {
                    module->column = MAX_COLUMNS;
                }
            }
        }
        return true;
    }

    if (gRubberBand.active == true) {
        convert_mouse_coord_to_module_area_coord(&gRubberBand.current, coord);
        return true;
    }
    return false;
}

bool canvas_empty_press(tCoord coord, bool additive) {
    tCoord moduleCoord = {0};

    if (within_rectangle(coord, module_area()) == false) {
        return false;
    }

    if (additive == false) {
        selection_clear();
    }
    convert_mouse_coord_to_module_area_coord(&moduleCoord, coord);
    gRubberBand.start   = moduleCoord;
    gRubberBand.current = moduleCoord;
    gRubberBand.active  = true;
    return true;
}

bool canvas_rubber_band_release(tCoord coord, uint32_t slot, uint32_t location, bool additive) {
    tCoord     moduleCoord = {0};
    tRectangle selRect     = {0};
    double     x1          = 0.0;
    double     y1          = 0.0;
    double     x2          = 0.0;
    double     y2          = 0.0;

    if (gRubberBand.active == false) {
        return false;
    }
    convert_mouse_coord_to_module_area_coord(&moduleCoord, coord);

    x1                 = (gRubberBand.start.x < moduleCoord.x) ? gRubberBand.start.x : moduleCoord.x;
    y1                 = (gRubberBand.start.y < moduleCoord.y) ? gRubberBand.start.y : moduleCoord.y;
    x2                 = (gRubberBand.start.x > moduleCoord.x) ? gRubberBand.start.x : moduleCoord.x;
    y2                 = (gRubberBand.start.y > moduleCoord.y) ? gRubberBand.start.y : moduleCoord.y;
    selRect            = (tRectangle){{
                                          x1, y1
                                      }, {
                                          x2 - x1, y2 - y1
                                      }
    };

    if (additive == false) {
        selection_clear();
    }
    selection_add_rect(selRect, slot, location);
    gRubberBand.active = false;
    return true;
}

bool canvas_module_drag_release(void) {
    if (gModuleDrag.active == false) {
        return false;
    }

    // RE-ORDER FIRST. A module dropped on top of another must push it down its column, exactly as
    // the application does on release — without this a drag leaves two modules occupying the same
    // grid squares, drawn over each other. Selected modules are transparent to one another, so a
    // multiple selection shuffles only what it lands on.
    if (gModuleDrag.isMulti) {
        shift_selection_down();
    } else {
        shift_modules_down(gModuleDrag.moduleKey);
    }
    gModuleDrag.active = false;
    return true;
}
