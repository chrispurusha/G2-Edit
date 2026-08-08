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
#include "msgQueue.h"
#include "protocol.h"
#include "undo.h"
#include "moduleResourcesAccess.h"
#include "paramOverlay.h"
#include "synthlibGlobals.h"
#include "canvasDrag.h"

void canvas_drag_set_origin(double rawX, double rawY) {
    gDragStartX = rawX;
    gDragStartY = rawY;
    gDragPrevX  = rawX;
    gDragPrevY  = rawY;
}

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

// ── Parameter (dial) dragging ───────────────────────────────────────────────────────────────────
//
// Lifted whole from cursor_pos()'s gParamDragging arm. Unchanged in behaviour; what changed is only
// how it learns three things it used to read from GLFW directly:
//
//   coord        the pointer in canvas logical units, as before
//   rawX/rawY    the RAW cursor position, which the vertical and horizontal dial modes difference
//                against their previous value. Rotary does not use them — it reads an absolute
//                angle each event — which is why the plug-in works today reporting rotary while
//                start_cursor_drag()'s cursor hiding and warping remains application-only.
//   altHeld      Alt drags the MORPH OFFSET rather than the value.
//
// Returns true if a parameter drag consumed the motion.
bool canvas_param_drag_motion(tCoord coord, double rawX, double rawY, bool altHeld) {
    double          x              = coord.x;
    double          y              = coord.y;
    double          xCoord         = rawX;
    double          yCoord         = rawY;
    double          angle          = 0.0;
    uint32_t        range          = 0;
    uint32_t        value          = 0;
    tMessageContent messageContent = {0};
    tParamType      paramType      = paramTypeCommonDial;
    uint32_t        slot           = gSlot;
    uint32_t        variation      = gPatchDescr[slot].activeVariation;

    (void)angle;
    (void)value;

    if (gParamDragging.active == false) {
        return false;
    }
    tModule *       module         = get_module(gParamDragging.moduleKey);

    if (module != NULL) {
        // Read and write through the dragged module's own Slot rather than the on-screen
        // gSlot the rest of this function uses. Identical for a drag on the patch canvas,
        // but a drag started in the Parameter Pages panel can be on a Global page knob
        // assigned to a module in one of the other three Slots, each with its own active
        // Variation.
        slot      = module->key.slot;
        variation = gPatchDescr[slot].activeVariation;

        switch (gParamDragging.type3) {
            case paramType3Param:

                if (module->key.location == locationMorph) {
                    paramType = paramTypeCommonDial;
                } else {
                    paramType = paramLocationList[module->param[variation][gParamDragging.param].paramRef].type;
                }

                if (  paramType != paramTypeToggle && paramType != paramTypeMenu
                   && paramType != paramTypeBypass && paramType != paramTypeEnable
                   && paramType != paramTypePush) {
                    if (module->key.location == locationMorph) {
                        range     = 128;
                        paramType = paramTypeCommonDial;
                    } else {
                        range     = paramLocationList[module->param[variation][gParamDragging.param].paramRef].range;
                        paramType = paramLocationList[module->param[variation][gParamDragging.param].paramRef].type;
                    }
                    // Alt held = setting the morph offset rather than the value itself
                    // (see below): module->param[...].value deliberately stays fixed
                    // while dragging like that, so the delta must be measured from the
                    // drag's fixed start point (gDragStartX/Y), not the continuously-
                    // reset gDragPrevX/Y — otherwise each event only reflects the tiny
                    // motion since the *previous* event against an unmoving base, which
                    // collapses to ~0 (and the morph amount flickers back to 0) the
                    // instant the mouse isn't actively moving between two polls. Rotary
                    // doesn't have this problem since it reads an absolute angle each
                    // event instead of an incremental delta.
                    bool    altHeld       = (altHeld);

                    // Continue adjusting from the morph offset that was already there at
                    // drag-start, rather than snapping it back to 0 the moment a new
                    // Alt-drag begins — same wraparound decoding used below when writing it.
                    int32_t altBaseOffset = 0;

                    if (altHeld) {
                        altBaseOffset = (gParamDragging.startMorphRange < 128)
                                            ? (int32_t)gParamDragging.startMorphRange
                                            : (int32_t)gParamDragging.startMorphRange - 256;
                    }

                    if (paramType == paramTypeSlider) {
                        double refY   = altHeld ? gDragStartY : gDragPrevY;
                        int    newVal = (int)module->param[variation][gParamDragging.param].value + altBaseOffset + (int)((refY - yCoord) * (double)range / 200.0);
                        gDragPrevY = yCoord;

                        if (newVal < 0) {
                            newVal = 0;
                        }

                        if (newVal >= (int)range) {
                            newVal = (int)range - 1;
                        }
                        value      = (uint32_t)newVal;
                    } else if (synthlib_dial_mode() == eDialModeVertical) {
                        double refY   = altHeld ? gDragStartY : gDragPrevY;
                        int    newVal = (int)module->param[variation][gParamDragging.param].value + altBaseOffset + (int)((refY - yCoord) * (double)range / 200.0);
                        gDragPrevY = yCoord;

                        if (newVal < 0) {
                            newVal = 0;
                        }

                        if (newVal >= (int)range) {
                            newVal = (int)range - 1;
                        }
                        value      = (uint32_t)newVal;
                    } else if (synthlib_dial_mode() == eDialModeHorizontal) {
                        double refX   = altHeld ? gDragStartX : gDragPrevX;
                        int    newVal = (int)module->param[variation][gParamDragging.param].value + altBaseOffset + (int)((xCoord - refX) * (double)range / 200.0);
                        gDragPrevX = xCoord;

                        if (newVal < 0) {
                            newVal = 0;
                        }

                        if (newVal >= (int)range) {
                            newVal = (int)range - 1;
                        }
                        value      = (uint32_t)newVal;
                    } else {
                        angle = calculate_mouse_angle((tCoord){x, y}, gParamRectangle[module->key.slot][module->key.location][module->key.index][gParamDragging.param]);
                        value = angle_to_value(angle, range);
                    }

                    if (!altHeld) {
                        if (module->param[variation][gParamDragging.param].value != value) {
                            module->param[variation][gParamDragging.param].value = value;
                            send_param_value(slot, gParamDragging.moduleKey, gParamDragging.param, variation, value);
                        }
                    } else {
                        uint32_t baseValue = module->param[variation][gParamDragging.param].value;
                        uint8_t  newMorphRange;

                        if (value >= baseValue) {
                            newMorphRange = (uint8_t)(value - baseValue);
                        } else {
                            newMorphRange = (uint8_t)(256 - (baseValue - value));
                        }

                        if (module->param[variation][gParamDragging.param].morphRange[gMorphGroupFocus] != newMorphRange) {
                            module->param[variation][gParamDragging.param].morphRange[gMorphGroupFocus] = newMorphRange;
                            LOG_DEBUG("Write to module %u variation %u\n", module->key.index, variation);

                            messageContent.cmd                                                          = eMsgCmdSetParamMorph;
                            messageContent.slot                                                         = slot;
                            messageContent.paramMorphData.moduleKey                                     = module->key;
                            messageContent.paramMorphData.param                                         = gParamDragging.param;
                            messageContent.paramMorphData.paramMorph                                    = gMorphGroupFocus;
                            messageContent.paramMorphData.value                                         = module->param[variation][gParamDragging.param].morphRange[gMorphGroupFocus];
                            messageContent.paramMorphData.negative                                      = 0;
                            messageContent.paramMorphData.variation                                     = variation;
                            msg_send(&gToUsbThread, &messageContent);
                        }
                    }
                }
                break;
            case paramType3Mode:

                if (  modeLocationList[module->mode[gParamDragging.mode].modeRef].type != paramTypeToggle
                   && modeLocationList[module->mode[gParamDragging.mode].modeRef].type != paramTypeMenu) {
                    uint32_t modeRange = modeLocationList[module->mode[gParamDragging.mode].modeRef].range;

                    if (synthlib_dial_mode() == eDialModeVertical) {
                        int newVal = (int)module->mode[gParamDragging.mode].value + (int)((gDragPrevY - yCoord) * (double)modeRange / 200.0);
                        gDragPrevY = yCoord;

                        if (newVal < 0) {
                            newVal = 0;
                        }

                        if (newVal >= (int)modeRange) {
                            newVal = (int)modeRange - 1;
                        }
                        value      = (uint32_t)newVal;
                    } else if (synthlib_dial_mode() == eDialModeHorizontal) {
                        int newVal = (int)module->mode[gParamDragging.mode].value + (int)((xCoord - gDragPrevX) * (double)modeRange / 200.0);
                        gDragPrevX = xCoord;

                        if (newVal < 0) {
                            newVal = 0;
                        }

                        if (newVal >= (int)modeRange) {
                            newVal = (int)modeRange - 1;
                        }
                        value      = (uint32_t)newVal;
                    } else {
                        angle = calculate_mouse_angle((tCoord){x, y}, module->mode[gParamDragging.mode].rectangle);
                        value = angle_to_value(angle, modeRange);
                    }

                    if (module->mode[gParamDragging.mode].value != value) {
                        module->mode[gParamDragging.mode].value = value;
                        send_mode_value(slot, gParamDragging.moduleKey, gParamDragging.mode, value);
                    }
                }
                break;
        }
    }
    return true;
}

// Ends a parameter drag: records it for undo, then clears the drag state.
//
// Extracted from finish_param_drag() so the plug-in can end a drag too. Without this the plug-in
// never cleared gParamDragging, so releasing the mouse left the dial "held" — and the next click
// anywhere carried on dragging it. The application still calls stop_dragging() afterwards, which
// clears the other drag kinds and restores the cursor; neither is meaningful here.
bool canvas_param_drag_release(void) {
    if (gParamDragging.active == false) {
        return false;
    }

    // Push the undo before stop_dragging() zeros gParamDragging
    if (gParamDragging.active) {
        tModule * pdMod = get_module(gParamDragging.moduleKey);

        if (pdMod) {
            // The dragged module's own Slot, not gSlot - a Global parameter page can hold a knob
            // assigned to a module in a Slot other than the one on screen.
            uint32_t pdVariation = gPatchDescr[gParamDragging.moduleKey.slot].activeVariation;

            if (gParamDragging.type3 == paramType3Param) {
                uint32_t curVal = pdMod->param[pdVariation][gParamDragging.param].value;
                undo_push_param_change(gParamDragging.moduleKey,
                                       gParamDragging.param,
                                       pdVariation,
                                       gParamDragging.startValue,
                                       curVal);
            } else {
                uint32_t curVal = pdMod->mode[gParamDragging.mode].value;
                undo_push_mode_change(gParamDragging.moduleKey,
                                      gParamDragging.mode,
                                      gParamDragging.startValue,
                                      curVal);
            }
        }
    }
    memset(&gParamDragging, 0, sizeof(gParamDragging));
    return true;
}
