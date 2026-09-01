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
#include "menus.h"
#include "moduleGraphics.h"
#include "cableChain.h"
#include "utils.h"
#include "splitView.h"
#include "mouseHandle.h"
#include "canvasDrag.h"

// Where a cable's loose end goes for a pointer at coord. The single definition of it: the press, the
// motion and the plug-in's own motion path all reach it here, and they did not previously agree.
//
// THE ORDER MATTERS AND WAS WRONG. The half-connector offset centres the cable on the pointer rather
// than hanging it from the connector square's top-left corner, and it is in MODULE SPACE — 8.75
// units, off a fixed MODULE_WIDTH of 350. Subtracting it from the SCREEN coordinate first meant
// convert_mouse_coord_to_module_area_coord() then divided it by the zoom factor along with
// everything else, while render_cable_from_to() adds the same offset back unscaled. The two only
// cancel at 100%: the end lands 8.75 * (zoom - 1) logical pixels from the pointer, trailing it when
// zoomed out and leading it when zoomed in. Converting first and offsetting after keeps the offset
// in the space it is expressed in.
void cable_drag_set_end(tCoord coord) {
    convert_mouse_coord_to_module_area_coord(&gCableDrag.toConnector.coord, coord);

    gCableDrag.toConnector.coord.x -= scale_from_percent(CONNECTOR_SIZE / 2.0);
    gCableDrag.toConnector.coord.y -= scale_from_percent(CONNECTOR_SIZE / 2.0);
}

void canvas_drag_set_origin(double rawX, double rawY) {
    gDragStartX = rawX;
    gDragStartY = rawY;
    gDragPrevX  = rawX;
    gDragPrevY  = rawY;
}

// Whole parameter units for a pointer movement of `pixels`, carrying the sub-unit remainder over to
// the next call — see tParamDragging::unitAccum for why discarding it made slow drags do nothing.
//
// ONLY FOR THE INCREMENTAL FORM, where the reference point advances every event. An Alt (morph) drag
// measures from the drag's fixed start instead, so its truncation loses nothing and it must NOT feed
// this accumulator: adding an absolute displacement to a running total every event would race away.
static int drag_whole_units(double pixels, uint32_t range) {
    gParamDragging.unitAccum += pixels * (double)range / dial_drag_pixels_for_full_range(range);

    int step = (int)gParamDragging.unitAccum;

    gParamDragging.unitAccum -= (double)step;
    return step;
}

// See canvasDrag.h: the origin FIRST, so that a shell whose cursor_capture() does nothing still gets
// working incremental dial drags. That ordering is the whole lesson of this split.
void canvas_drag_begin(void) {
    double rawX = 0.0;
    double rawY = 0.0;

    cursor_raw_coord(&rawX, &rawY);
    canvas_drag_set_origin(rawX, rawY);
    cursor_capture();
}

// One gesture, one function. These three were the three if-blocks of canvas_drag_motion(), which is
// now a thin wrapper over them — see the gesture table below.
static bool module_drag_motion(tCoord coord) {
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
            //
            // AND THE DELTA IS CLAMPED ONCE FOR THE WHOLE GROUP, not each module against the grid
            // separately. Clamping per module was the bug (CT, 2026-08-30: "relative position of the
            // group to each other should remain the same. currently, individuals can reposition vs
            // the rest"): drag a selection at the left edge and the members already in column 0 stay
            // put while the rest keep moving, so the shape of the selection is permanently deformed.
            // The clamp belongs on the movement, not on the destination.
            if ((dc != 0) || (dr != 0)) {
                int32_t minCol = (int32_t)MAX_COLUMNS;
                int32_t minRow = (int32_t)MAX_ROWS;
                int32_t maxCol = 0;
                int32_t maxRow = 0;

                for (uint32_t i = 0; i < gSelection.count; i++) {
                    tModule * sel = get_module(gSelection.keys[i]);

                    if (sel == NULL) {
                        continue;
                    }

                    if ((int32_t)sel->column < minCol) {
                        minCol = (int32_t)sel->column;
                    }

                    if ((int32_t)sel->column > maxCol) {
                        maxCol = (int32_t)sel->column;
                    }

                    if ((int32_t)sel->row < minRow) {
                        minRow = (int32_t)sel->row;
                    }

                    if ((int32_t)sel->row > maxRow) {
                        maxRow = (int32_t)sel->row;
                    }
                }

                // The furthest the whole group may travel before any member leaves the grid.
                if (dc < -minCol) {
                    dc = -minCol;
                }

                if (dc > ((int32_t)MAX_COLUMNS - maxCol)) {
                    dc = (int32_t)MAX_COLUMNS - maxCol;
                }

                if (dr < -minRow) {
                    dr = -minRow;
                }

                if (dr > ((int32_t)MAX_ROWS - maxRow)) {
                    dr = (int32_t)MAX_ROWS - maxRow;
                }

                for (uint32_t i = 0; i < gSelection.count; i++) {
                    tModule * sel = get_module(gSelection.keys[i]);

                    if (sel == NULL) {
                        continue;
                    }
                    sel->column = (uint32_t)((int32_t)sel->column + dc);
                    sel->row    = (uint32_t)((int32_t)sel->row + dr);
                }

                // prev follows the CURSOR, not the (possibly clamped) movement. Banking the refused
                // travel would make the group feel stuck: having pushed it into the left edge, the
                // user would have to drag back through all that refused distance before it moved.
                gModuleDrag.prevColumn = newCol;
                gModuleDrag.prevRow    = newRow;
            }
        } else {
            tModule * module = get_module(gModuleDrag.moduleKey);

            if (module != NULL) {
                // BY THE DELTA, exactly as the multi path above does, so the module keeps the offset
                // it was grabbed by. This used to assign the cursor's cell straight into
                // column/row, which put the module's TOP-LEFT under the pointer and made it jump the
                // moment a drag started anywhere but the title strip.
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

                if ((dc != 0) || (dr != 0)) {
                    int32_t nc = (int32_t)module->column + dc;
                    int32_t nr = (int32_t)module->row + dr;

                    module->column         = (uint32_t)((nc < 0) ? 0 : ((nc > (int32_t)MAX_COLUMNS) ? (int32_t)MAX_COLUMNS : nc));
                    module->row            = (uint32_t)((nr < 0) ? 0 : ((nr > (int32_t)MAX_ROWS) ? (int32_t)MAX_ROWS : nr));
                    gModuleDrag.prevColumn = newCol;
                    gModuleDrag.prevRow    = newRow;
                }
            }
        }
        return true;
    }
    return false;
}

static bool cable_drag_motion(tCoord coord) {
    if (gCableDrag.active == true) {
        cable_drag_set_end(coord);
        return true;
    }
    return false;
}

static bool rubber_band_motion(tCoord coord) {
    if (gRubberBand.active == true) {
        convert_mouse_coord_to_module_area_coord(&gRubberBand.current, coord);
        return true;
    }
    return false;
}

// KEPT for the callers that just want "carry whatever drag is in progress" without describing the
// event — the plug-in's drag tick is one. New code should prefer canvas_gesture_motion().
bool canvas_drag_motion(tCoord coord) {
    return module_drag_motion(coord) || cable_drag_motion(coord) || rubber_band_motion(coord);
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
    //
    // A column packed to MAX_ROWS has nowhere to put the drop, and the shift says so rather than
    // piling modules onto the last row. The whole location goes back to where it was at drag start -
    // the snapshot canvas_module_drag_begin() already takes for undo serves as the rollback, which is
    // why it covers the location and not just the dragged keys.
    bool placed = gModuleDrag.isMulti ? shift_selection_down() : shift_modules_down(gModuleDrag.moduleKey);

    if (placed == false) {
        for (uint32_t i = 0; i < gModuleDrag.snapshotCount; i++) {
            tModule * walk = get_module(gModuleDrag.snapshotKeys[i]);

            if (walk == NULL) {
                continue;
            }

            if ((walk->column != gModuleDrag.snapshotColumn[i]) || (walk->row != gModuleDrag.snapshotRow[i])) {
                walk->column = gModuleDrag.snapshotColumn[i];
                walk->row    = gModuleDrag.snapshotRow[i];

                // Told to the G2 as well. A multiple selection can fail on its third member with the
                // first two already moved and their moves already sent, so the restore cannot assume
                // the device is still holding the pre-drag layout.
                send_module_move_msg(walk);
            }
        }
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
//                cursor_capture()'s pointer hiding remains application-only — see canvasDrag.h.
//   altHeld      Alt drags the MORPH OFFSET rather than the value.
//
// Returns true if a parameter drag consumed the motion.
bool canvas_param_drag_motion(tCoord coord, double rawX, double rawY, bool altHeld) {
    double     x         = coord.x;
    double     y         = coord.y;
    double     xCoord    = rawX;
    double     yCoord    = rawY;
    double     angle     = 0.0;
    uint32_t   range     = 0;
    uint32_t   value     = 0;
    tParamType paramType = paramTypeCommonDial;
    uint32_t   slot      = gSlot;
    uint32_t   variation = gPatchDescr[slot].activeVariation;

    (void)angle;
    (void)value;

    if (gParamDragging.active == false) {
        return false;
    }
    tModule *  module    = get_module(gParamDragging.moduleKey);

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
                   && paramType != paramTypePush && paramType != paramTypeRadioEdit) {
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
                    // NO LOCAL altHeld HERE. There used to be `bool altHeld = (altHeld);` — a local
                    // shadowing the parameter and initialised FROM ITSELF, so it read an
                    // uninitialised stack slot and the caller's answer was thrown away. Undefined
                    // behaviour that looks stable: the garbage byte happens to be whatever the
                    // previous call left at that stack offset, so it can read false for months and
                    // then turn true when an unrelated change alters the frame above it. That is
                    // exactly what happened — removing two now-unused locals from cursor_pos() made
                    // every plain dial drag start writing the morph offset. Use the parameter.

                    // Continue adjusting from the morph offset that was already there at
                    // drag-start, rather than snapping it back to 0 the moment a new
                    // Alt-drag begins — same wraparound decoding used below when writing it.
                    int32_t altBaseOffset = 0;

                    if (altHeld) {
                        altBaseOffset = (gParamDragging.startMorphRange < 128)
                                            ? (int32_t)gParamDragging.startMorphRange
                                            : (int32_t)gParamDragging.startMorphRange - 256;
                    }

                    // SHIFT SLOWS THE DRAG DOWN — dial_drag_pixels_for_full_range() (SynthLib) is the
                    // shared policy, the same one SynthEdit's dials use, so "finer" means the same
                    // thing in both editors. Modes are left out below: a 2-to-4 position selector
                    // gains nothing from a finer drag.
                    if (paramType == paramTypeSlider) {
                        double refY   = altHeld ? gDragStartY : gDragPrevY;
                        int    newVal = (int)module->param[variation][gParamDragging.param].value + altBaseOffset
                                        + (altHeld ? (int)((refY - yCoord) * (double)range / dial_drag_pixels_for_full_range(range))
                                                   : drag_whole_units(refY - yCoord, range));
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
                        int    newVal = (int)module->param[variation][gParamDragging.param].value + altBaseOffset
                                        + (altHeld ? (int)((refY - yCoord) * (double)range / dial_drag_pixels_for_full_range(range))
                                                   : drag_whole_units(refY - yCoord, range));
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
                        int    newVal = (int)module->param[variation][gParamDragging.param].value + altBaseOffset
                                        + (altHeld ? (int)((xCoord - refX) * (double)range / dial_drag_pixels_for_full_range(range))
                                                   : drag_whole_units(xCoord - refX, range));
                        gDragPrevX = xCoord;

                        if (newVal < 0) {
                            newVal = 0;
                        }

                        if (newVal >= (int)range) {
                            newVal = (int)range - 1;
                        }
                        value      = (uint32_t)newVal;
                    } else {
                        angle = calculate_mouse_angle((tCoord){x, y}, gParamDragging.rect);   // captured at press — see tParamDragging
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
                            send_param_morph(slot, module->key, gParamDragging.param, gMorphGroupFocus, variation,
                                             module->param[variation][gParamDragging.param].morphRange[gMorphGroupFocus]);
                        }
                    }
                }
                break;
            case paramType3Mode:

                if (  modeLocationList[module->mode[gParamDragging.mode].modeRef].type != paramTypeToggle
                   && modeLocationList[module->mode[gParamDragging.mode].modeRef].type != paramTypeMenu) {
                    uint32_t modeRange = modeLocationList[module->mode[gParamDragging.mode].modeRef].range;

                    if (synthlib_dial_mode() == eDialModeVertical) {
                        int newVal = (int)module->mode[gParamDragging.mode].value + drag_whole_units(gDragPrevY - yCoord, modeRange);
                        gDragPrevY = yCoord;

                        if (newVal < 0) {
                            newVal = 0;
                        }

                        if (newVal >= (int)modeRange) {
                            newVal = (int)modeRange - 1;
                        }
                        value      = (uint32_t)newVal;
                    } else if (synthlib_dial_mode() == eDialModeHorizontal) {
                        int newVal = (int)module->mode[gParamDragging.mode].value + drag_whole_units(xCoord - gDragPrevX, modeRange);
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

                // Linked variations pick up the SETTLED value, once, rather than every intermediate
                // one the drag passed through — see send_param_value_to_links(). None of them is on
                // screen during the drag, so there is nothing to keep in step until here.
                send_param_value_to_links(gParamDragging.moduleKey.slot, gParamDragging.moduleKey,
                                          gParamDragging.param, pdVariation, curVal);
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

// ── Right-click menus ───────────────────────────────────────────────────────────────────────────
//
// The canvas half of mouseHandle.c's mouseButtonRightUp handler, lifted out so the plug-in gets the
// same menus. Hit-tests in the application's order — connectors, then parameters, then the module
// body, then the morph labels — which matters: a connector sits inside its module's rectangle, so
// testing the body first would swallow every connector right-click.
//
// The application keeps its own topbar and module-area right-click handling after calling this.
bool canvas_right_click(tCoord coord, uint32_t slot, uint32_t location) {
    bool found = false;

    // ONE QUERY, not a walk over every module and every parameter. The click-region registry already
    // holds each widget's rectangle and its identity, front to back, so "what is under the cursor"
    // is a lookup rather than a re-derivation — and it is the SAME lookup a left-click makes, which
    // is the property the old nested loops could not offer: they were a second opinion about z-order
    // that happened to agree.
    //
    // The precedence this replaces is preserved without being restated. Connectors and parameters
    // register AFTER the module body (render_module registers the body, then the drag strip, then
    // calls render_module_common), and the registry resolves ties by taking the most recently
    // registered — so a connector still wins over the body it sits inside, exactly as the old
    // "connectors, then params, then body" order spelled out.
    //
    // THE SLOT AND LOCATION FILTER IS NOT OPTIONAL, and dropping it was a real regression: with the
    // split view showing the Voice Area and the FX area at once, both panes have widgets registered,
    // and a right-click in the FX pane came back with a VA module — the owner saw an "Assign knob"
    // menu for a module underneath. The old nested walk was implicitly scoped because it iterated
    // only the modules of the location it was given; a registry query is scoped to the whole screen,
    // so the scope has to be stated. Ask the registry WHAT is there, then confirm it is something
    // this pane owns.
    const tCanvasWidget * widget = canvas_widget_at(coord);

    if ((widget != NULL) && ((widget->key.slot != slot) || (widget->key.location != location))) {
        widget = NULL;
    }

    if (widget != NULL) {
        found = true;

        switch (widget->kind) {
            case eCanvasWidgetConnector:
                open_connector_context_menu(coord, widget->key, (int)canvas_widget_index(widget));
                break;

            case eCanvasWidgetParam:
                open_param_context_menu(coord, widget->key, canvas_widget_index(widget));
                break;

            case eCanvasWidgetModule:
                // The drag strip shares the module's context, so a right-click there opens the
                // module menu — which is what it did before, by falling through to the body.
                open_module_context_menu(coord, widget->key);
                break;

            case eCanvasWidgetMode:
            case eCanvasWidgetMorph:
            case eCanvasWidgetNone:
            default:
                // A mode selector has no right-click menu of its own; fall back to its module's,
                // which is where a right-click on that part of the face used to land.
                open_module_context_menu(coord, widget->key);
                break;
        }
    }

    if (found == false) {
        // The morph LABELS are drawn text, not registered widgets, so they are still a rectangle
        // test. Worth registering one day; not worth inventing a widget kind for it today.
        for (int mi = 0; mi < NUM_MORPHS && !found; mi++) {
            if (within_rectangle(coord, gMorphLabelRect[mi])) {
                open_morph_label_context_menu(coord, (uint32_t)mi);
                found = true;
            }
        }
    }
    return found;
}

// ── Cable dragging ──────────────────────────────────────────────────────────────────────────────
//
// The press already lives in a click-region handler (connector_click_handler in moduleGraphics.c),
// which the plug-in shares; what follows is the motion's destination and the connect on release,
// moved out of mouseHandle.c so the plug-in can patch as well as look.
//
// msg_send() inside the connect tells the G2 about the new cable. In a plug-in that reaches a stub
// and does nothing, which is right — the cable exists locally, the sound engine picks it up from the
// database, and no hardware is written to.

void set_up_cable_key(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex) {
    // This logic is pretty horrible - sorry
    cableKey->slot                 = fromModule->key.slot;
    cableKey->location             = fromModule->key.location;
    cableKey->moduleFromIndex      = fromModule->key.index;
    cableKey->connectorFromIoCount = find_io_count_from_index(fromModule, fromModule->connector[gCableDrag.fromConnectorIndex].dir, gCableDrag.fromConnectorIndex);
    cableKey->moduleToIndex        = toModule->key.index;
    cableKey->connectorToIoCount   = find_io_count_from_index(toModule, toModule->connector[toConnectorIndex].dir, toConnectorIndex);
    cableKey->linkType             = fromModule->connector[gCableDrag.fromConnectorIndex].dir;
}

bool swap_cable_to_from_if_needed(tCableKey * cableKey, tModule * fromModule, tModule * toModule, int toConnectorIndex) {
    if (  fromModule->connector[gCableDrag.fromConnectorIndex].dir == connectorDirIn
       && toModule->connector[toConnectorIndex].dir == connectorDirOut) {
        uint32_t tmpModuleIndex    = cableKey->moduleFromIndex;
        uint32_t tmpConnectorIndex = cableKey->connectorFromIoCount;

        cableKey->moduleFromIndex      = cableKey->moduleToIndex;
        cableKey->connectorFromIoCount = cableKey->connectorToIoCount;
        cableKey->moduleToIndex        = tmpModuleIndex;
        cableKey->connectorToIoCount   = tmpConnectorIndex;
        cableKey->linkType             = toModule->connector[toConnectorIndex].dir;

        return true; // Indicates swap occurred
    }
    return false;
}

// Does this cable have an end plugged into that hole? Either end can match: linkType says which
// direction the FROM end points, which is what makes an input-to-input link (two input ends, see the
// backdoor-duplicate note in Docs) readable here rather than guessed at.
bool cable_touches_connector(const tCable * cable, uint32_t moduleIndex, uint32_t ioCount, tConnectorDir dir) {
    if ((cable == NULL) || !cable->active) {
        return false;
    }

    // The TO end is always an input.
    if (  (cable->key.moduleToIndex == moduleIndex) && (cable->key.connectorToIoCount == ioCount)
       && (dir == connectorDirIn)) {
        return true;
    }
    return (cable->key.moduleFromIndex == moduleIndex) && (cable->key.connectorFromIoCount == ioCount)
           && ((tConnectorDir)cable->key.linkType == dir);
}

// The far end of a cable, given which end is plugged into the hole being moved.
void cable_far_end(const tCable * cable, uint32_t moduleIndex, uint32_t ioCount,
                   uint32_t * farModuleIndex, uint32_t * farIoCount, tConnectorDir * farDir) {
    if ((cable->key.moduleToIndex == moduleIndex) && (cable->key.connectorToIoCount == ioCount)) {
        *farModuleIndex = cable->key.moduleFromIndex;
        *farIoCount     = cable->key.connectorFromIoCount;
        *farDir         = (tConnectorDir)cable->key.linkType;
        return;
    }
    *farModuleIndex = cable->key.moduleToIndex;
    *farIoCount     = cable->key.connectorToIoCount;
    *farDir         = connectorDirIn;
}

// Builds the key for a cable between two holes, in the form the database and the wire both expect:
// the TO end is always an input and the FROM end's direction is carried in linkType. Two outputs
// cannot be joined, which is the one combination this refuses.
static bool make_cable_key(uint32_t slot, uint32_t location,
                           uint32_t aModule, uint32_t aIo, tConnectorDir aDir,
                           uint32_t bModule, uint32_t bIo, tConnectorDir bDir,
                           tCableKey * key) {
    if ((aDir == connectorDirOut) && (bDir == connectorDirOut)) {
        return false;
    }
    bool aIsFrom = (bDir == connectorDirIn);   // two inputs is a legal white link; a is the from end

    key->slot                 = slot;
    key->location             = location;
    key->moduleFromIndex      = aIsFrom ? aModule : bModule;
    key->connectorFromIoCount = aIsFrom ? aIo : bIo;
    key->linkType             = (uint32_t)(aIsFrom ? aDir : bDir);
    key->moduleToIndex        = aIsFrom ? bModule : aModule;
    key->connectorToIoCount   = aIsFrom ? bIo : aIo;

    return true;
}

// The cable attached to a given connector, and where its OTHER end is — what Ctrl-click needs in
// order to pick a cable up and drag its free end. Either end can be the one clicked: linkType says
// which direction the FROM end points, which is what makes an input-to-input link (two input ends,
// see the backdoor-duplicate note in Docs) readable here rather than guessed at.
//
// An output can carry several cables. This takes the FIRST it finds, which is deterministic but
// arbitrary; the original editor has the same ambiguity and the manual does not say how it resolves
// it. For an input there is only ever one, which is the case that matters.
bool find_cable_at_connector(uint32_t slot, uint32_t location, uint32_t moduleIndex,
                             uint32_t ioCount, tConnectorDir dir,
                             tCableKey * key, uint32_t * otherModuleIndex,
                             uint32_t * otherIoCount, tConnectorDir * otherDir) {
    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, location, i);

        if ((cable == NULL) || !cable->active) {
            continue;
        }

        // The TO end is always an input.
        if (  (cable->key.moduleToIndex == moduleIndex) && (cable->key.connectorToIoCount == ioCount)
           && (dir == connectorDirIn)) {
            *key              = cable->key;
            *otherModuleIndex = cable->key.moduleFromIndex;
            *otherIoCount     = cable->key.connectorFromIoCount;
            *otherDir         = (tConnectorDir)cable->key.linkType;
            return true;
        }

        if (  (cable->key.moduleFromIndex == moduleIndex) && (cable->key.connectorFromIoCount == ioCount)
           && ((tConnectorDir)cable->key.linkType == dir)) {
            *key              = cable->key;
            *otherModuleIndex = cable->key.moduleToIndex;
            *otherIoCount     = cable->key.connectorToIoCount;
            *otherDir         = connectorDirIn;
            return true;
        }
    }

    return false;
}

static bool input_connector_has_cable(uint32_t slot, uint32_t location,
                                      uint32_t moduleIndex, uint32_t ioCount) {
    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, location, i);

        if (cable == NULL || !cable->active) {
            continue;
        }

        if (cable->key.moduleToIndex == moduleIndex && cable->key.connectorToIoCount == ioCount) {
            return true;
        }
    }

    return false;
}

// Moving a picked-up hole and everything plugged into it. See tCableDragging: Ctrl-click grabs a
// CONNECTOR, not a cable, and the original moves the lot as one operation.
//
// ALL OR NOTHING when the drop lands on a connector. Every cable is validated before any is deleted,
// and if one of them cannot be made — most obviously three cables dropped on an input, which accepts
// exactly one — nothing changes at all. A partial move would leave the patch in a state nobody asked
// for and would have to be unpicked by hand. Dropped on empty canvas, they are all disconnected,
// which is the manual's "pull out the connector and release".
static bool handle_cable_reroute(tCoord coord, uint32_t slot, uint32_t location) {
    tCableKey oldKeys[MAX_CABLES_PER_CONNECTOR] = {0};
    tCableKey newKeys[MAX_CABLES_PER_CONNECTOR] = {0};
    uint32_t  colours[MAX_CABLES_PER_CONNECTOR] = {0};
    uint32_t  count                             = 0;
    tModule * toModule                          = NULL;
    int32_t   toIndex                           = -1;

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(slot, location, i);

        if (!cable_touches_connector(cable, gCableDrag.rerouteModuleIndex, gCableDrag.rerouteIoCount, gCableDrag.rerouteDir)) {
            continue;
        }

        if (count >= MAX_CABLES_PER_CONNECTOR) {
            LOG_ERROR("More than %u cables on one connector — not moving any of them\n", MAX_CABLES_PER_CONNECTOR);
            return false;
        }
        oldKeys[count] = cable->key;
        colours[count] = cable->colour;
        count++;
    }

    if (count == 0) {
        return false;
    }

    // What is under the cursor, if anything.
    for (uint32_t idx = 0; (idx < MAX_NUM_MODULES) && (toIndex < 0); idx++) {
        tModule * module = get_module_slot(slot, location, idx);

        if ((module == NULL) || !module->active) {
            continue;
        }

        for (int32_t i = 0; i < (int32_t)module_connector_count(module->type); i++) {
            if (within_rectangle(coord, module->connector[i].rectangle)) {
                toModule = module;
                toIndex  = i;
                break;
            }
        }
    }

    if (toIndex >= 0) {
        tConnectorDir toDir = toModule->connector[toIndex].dir;
        int           toIo  = find_io_count_from_index(toModule, toDir, toIndex);

        if (toIo < 0) {
            return false;
        }

        for (uint32_t c = 0; c < count; c++) {
            uint32_t      farModule = 0;
            uint32_t      farIo     = 0;
            tConnectorDir farDir    = connectorDirIn;
            tCable *      cable     = get_cable(oldKeys[c]);

            if (cable == NULL) {
                return false;
            }
            cable_far_end(cable, gCableDrag.rerouteModuleIndex, gCableDrag.rerouteIoCount, &farModule, &farIo, &farDir);

            // Onto its own far end is a self-connection, and two outputs cannot be joined.
            if (  ((toModule->key.index == farModule) && ((uint32_t)toIo == farIo) && (toDir == farDir))
               || !make_cable_key(slot, location, toModule->key.index, (uint32_t)toIo, toDir,
                                  farModule, farIo, farDir, &newKeys[c])) {
                return false;
            }

            // The input at the TO end must be free — ignoring the cables about to be removed, and
            // counting the ones already claimed by this same move. That second part is what stops
            // several cables being dropped onto one input.
            for (uint32_t e = 0; e < c; e++) {
                if (  (newKeys[e].moduleToIndex == newKeys[c].moduleToIndex)
                   && (newKeys[e].connectorToIoCount == newKeys[c].connectorToIoCount)) {
                    return false;
                }
            }

            for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
                tCable * existing   = get_cable_slot(slot, location, i);

                if ((existing == NULL) || !existing->active) {
                    continue;
                }

                if (  (existing->key.moduleToIndex != newKeys[c].moduleToIndex)
                   || (existing->key.connectorToIoCount != newKeys[c].connectorToIoCount)) {
                    continue;
                }
                bool     beingMoved = false;

                for (uint32_t o = 0; o < count; o++) {
                    if (  (existing->key.moduleFromIndex == oldKeys[o].moduleFromIndex)
                       && (existing->key.connectorFromIoCount == oldKeys[o].connectorFromIoCount)
                       && (existing->key.moduleToIndex == oldKeys[o].moduleToIndex)
                       && (existing->key.connectorToIoCount == oldKeys[o].connectorToIoCount)) {
                        beingMoved = true;
                        break;
                    }
                }

                if (!beingMoved) {
                    return false;   // occupied by a cable that is staying put
                }
            }
        }
    }

    // Past every check: take them all out.
    for (uint32_t c = 0; c < count; c++) {
        tMessageContent msg = {0};

        msg.cmd                            = eMsgCmdDeleteCable;
        msg.slot                           = slot;
        msg.cableData.location             = location;
        msg.cableData.moduleFromIndex      = oldKeys[c].moduleFromIndex;
        msg.cableData.connectorFromIoIndex = oldKeys[c].connectorFromIoCount;
        msg.cableData.moduleToIndex        = oldKeys[c].moduleToIndex;
        msg.cableData.connectorToIoIndex   = oldKeys[c].connectorToIoCount;
        msg.cableData.linkType             = oldKeys[c].linkType;
        msg_send(&gToUsbThread, &msg);
        delete_cable(oldKeys[c]);
    }

    if (toIndex < 0) {
        // Dropped on nothing: they are simply gone, and what each was feeding has lost its source.
        for (uint32_t c = 0; c < count; c++) {
            cable_chain_recolour(slot, location, (tCableNode){
                oldKeys[c].moduleToIndex, oldKeys[c].connectorToIoCount, false
            });
        }

        return false;
    }

    for (uint32_t c = 0; c < count; c++) {
        tCable          cable = {0};
        tMessageContent msg   = {0};

        cable.colour                       = colours[c];
        write_cable(newKeys[c], &cable);

        msg.cmd                            = eMsgCmdWriteCable;
        msg.slot                           = slot;
        msg.cableData.location             = location;
        msg.cableData.moduleFromIndex      = newKeys[c].moduleFromIndex;
        msg.cableData.connectorFromIoIndex = newKeys[c].connectorFromIoCount;
        msg.cableData.moduleToIndex        = newKeys[c].moduleToIndex;
        msg.cableData.connectorToIoIndex   = newKeys[c].connectorToIoCount;
        msg.cableData.linkType             = newKeys[c].linkType;
        msg.cableData.colour               = cable.colour;
        msg_send(&gToUsbThread, &msg);
    }

    // Topology changed at both ends, so both chains are re-derived — see the note in the connect
    // path about why this is tied to topology changes only.
    for (uint32_t c = 0; c < count; c++) {
        cable_chain_recolour(slot, location, (tCableNode){
            oldKeys[c].moduleToIndex, oldKeys[c].connectorToIoCount, false
        });
        cable_chain_recolour(slot, location, (tCableNode){
            newKeys[c].moduleToIndex, newKeys[c].connectorToIoCount, false
        });
    }

    return true;
}

bool handle_cable_connect(tCoord coord, uint32_t slot, uint32_t location) {
    bool       found         = false;
    int32_t    i             = 0;
    tCableKey  cableKey      = {0};
    tCable     cable         = {0};
    bool       connected     = false;
    tCableNode connectedNode = {0};

    tModule *  fromModule    = get_module(gCableDrag.fromModuleKey);

    if (fromModule == NULL) {
        return false;
    }
    // Bracketed as one cable edit: a connect can repaint the whole tree as well as add the
    // cable, and undo has to put the old colours back along with the topology.
    undo_begin_cable_edit(slot, location);

    // A re-route is its own operation — see handle_cable_reroute(). It runs inside this bracket so
    // the whole move, however many cables it touches, is one undo step.
    if (gCableDrag.rerouting) {
        bool moved = handle_cable_reroute(coord, slot, location);

        update_module_up_rates();
        undo_commit_cable_edit();

        return moved;
    }

    for (uint32_t idx = 0; idx < MAX_NUM_MODULES && !found; idx++) {
        tModule * toModule = get_module_slot(slot, location, idx);

        if (!toModule->active) {
            continue;
        }

        for (i = 0; i < (int32_t)module_connector_count(toModule->type); i++) {
            if (within_rectangle(coord, toModule->connector[i].rectangle) == true) {
                found = true;
                set_up_cable_key(&cableKey, fromModule, toModule, i);

                swap_cable_to_from_if_needed(&cableKey, fromModule, toModule, i);

                // Prevent self-connections and invalid connections
                if (  (cableKey.moduleFromIndex == cableKey.moduleToIndex && gCableDrag.fromConnectorIndex == i)
                   || (  fromModule->connector[gCableDrag.fromConnectorIndex].dir == connectorDirOut
                      && toModule->connector[i].dir == connectorDirOut)) {
                    break;
                }

                // Note that this call will walk the cables, which we can't nest
                if (input_connector_has_cable(slot, location,
                                              cableKey.moduleToIndex,
                                              cableKey.connectorToIoCount)) {
                    break;
                }
                // Inherits the FROM connector's CURRENT (upRate-promoted, if applicable) colour, not
                // just its declared base type — matches the manual's "cables connected to this
                // output will inherit this colour" (g2manual.txt p.71); see effective_connector_type()'s
                // own comment (moduleResourcesAccess.h) for why the promotion itself lives there,
                // not in the stored connector type.
                tConnectorType  fromConnectorType = effective_connector_type(
                    fromModule->connector[gCableDrag.fromConnectorIndex].type, fromModule->upRate);
                cable.colour                                  = (uint32_t)cable_colour_for_connector_type(fromConnectorType);
                write_cable(cableKey, &cable);

                tMessageContent messageContent    = {0};

                messageContent.cmd                            = eMsgCmdWriteCable;
                messageContent.slot                           = slot;
                messageContent.cableData.location             = location;
                messageContent.cableData.moduleFromIndex      = cableKey.moduleFromIndex;
                messageContent.cableData.connectorFromIoIndex = cableKey.connectorFromIoCount;
                messageContent.cableData.moduleToIndex        = cableKey.moduleToIndex;
                messageContent.cableData.connectorToIoIndex   = cableKey.connectorToIoCount;
                messageContent.cableData.linkType             = cableKey.linkType;
                messageContent.cableData.colour               = cable.colour;
                msg_send(&gToUsbThread, &messageContent);

                // The to-end is always an input, so this needs no database lookup — which also
                // keeps it safe if write_cable() found no free slot
                connected                                     = true;
                connectedNode                                 = (tCableNode){
                    cableKey.moduleToIndex, cableKey.connectorToIoCount, false
                };

                break;
            }
        }
    }

    // A connect is a topology change, so the chain's colour is re-derived across the WHOLE tree,
    // exactly as GetConnectRecolorMolecules (G2Editor.c:159558) does with a CCompleteTreeIterator
    // — note COMPLETE, where the branch-scoped commands use CCompleteBranchIterator.
    //
    // This is what maintains the invariant that the colour above only guesses at: every cable in
    // a chain carries ONE colour, the source output's signal colour, or WHITE when the chain has
    // no source at all. Joining two inputs together produces a sourceless chain and so comes out
    // white, which is the manual's "non-functional input-to-input connections". Attaching a
    // source later repaints the whole tree, discarding any colour the user had chosen — which is
    // the original's behaviour, and the reason recolouring is tied to topology changes only.
    if (connected) {
        cable_chain_recolour(slot, location, connectedNode);
    }
    update_module_up_rates();
    undo_commit_cable_edit();  // Pushes nothing if the drag landed somewhere that added no cable

    return found;
}


// ── Auto-scroll while dragging ──────────────────────────────────────────────────────────────────
//
// Dragging a module or a cable past the edge of a pane scrolls that pane to follow. Moved out of
// mouseHandle.c once the plug-in gained scrollbars of its own — it was left behind on the first pass
// precisely because a plug-in with no scrollbars had nothing to scroll.
//
// Nothing in it was ever platform-bound: get_time_ms() is SynthLib's, and the rest is the pane
// machinery. The rate RAMPS from DRAG_SCROLL_MIN_RATE to DRAG_SCROLL_MAX_RATE across
// DRAG_SCROLL_RAMP_DIST of overshoot, so easing just past the edge creeps and pushing well beyond it
// moves quickly — which is what stops it feeling like a runaway.

// Distance in content pixels to scroll this tick, given how far past the pane edge the cursor is.
// Sign is the caller's; overshoot is always positive here.
static double drag_scroll_step(double overshoot, double seconds) {
    double ramp = overshoot / DRAG_SCROLL_RAMP_DIST;

    if (ramp > 1.0) {
        ramp = 1.0;
    }
    return (DRAG_SCROLL_MIN_RATE + ((DRAG_SCROLL_MAX_RATE - DRAG_SCROLL_MIN_RATE) * ramp)) * seconds;
}

void adjust_scroll_for_drag(void) {
    // Own timestamp rather than get_time_delta(): that function's static is shared with the USB
    // thread, which consumes part of the elapsed time out from under us at unpredictable moments.
    static double lastTimeMs = 0.0;
    double        nowMs      = get_time_ms();
    double        timeDelta  = (lastTimeMs == 0.0) ? 0.0 : (nowMs - lastTimeMs);
    tCoord        coord      = {0};
    uint32_t      pane       = split_view_focused_pane();
    tRectangle    area       = module_area_for_pane(pane);
    double        dx         = 0.0;
    double        dy         = 0.0;

    lastTimeMs = nowMs;

    if (timeDelta > DRAG_SCROLL_MAX_STEP_MS) {
        timeDelta = DRAG_SCROLL_MAX_STEP_MS;
    }
    // The rates are per second; the delta is in milliseconds.
    double        seconds    = timeDelta / 1000.0;

    if (seconds <= 0.0) {
        return;
    }
    get_global_gui_scaled_mouse_coord(&coord);

    // Dragging past a pane's edge scrolls THAT pane. Clamped to the pane the drag started in, so a
    // drag heading for the divider scrolls its own half rather than reaching into the other one.
    if (coord.x > (area.coord.x + area.size.w)) {
        dx = drag_scroll_step(coord.x - (area.coord.x + area.size.w), seconds);
    } else if (coord.x < area.coord.x) {
        dx = -drag_scroll_step(area.coord.x - coord.x, seconds);
    }

    if (coord.y > (area.coord.y + area.size.h)) {
        dy = drag_scroll_step(coord.y - (area.coord.y + area.size.h), seconds);
    } else if (coord.y < area.coord.y) {
        dy = -drag_scroll_step(area.coord.y - coord.y, seconds);
    }

    if ((dx != 0.0) || (dy != 0.0)) {
        pane_scroll_by(pane, dx, dy);
    }
}

// Right-click on EMPTY canvas: the create-module menu. Moved from mouseHandle.c so the plug-in
// gets it too — canvas_right_click() above handles everything that is on a module, and this is what
// the application calls next when none of it matched.
bool handle_module_area_click(tCoord coord) {
    if (within_rectangle(coord, module_area())) {
        open_module_area_context_menu(coord);
        return true;
    }
    return false;
}

// Which connector the pointer is over, if any. Lifted from cursor_pos()'s final branch.
//
// The canvas dims every cable NOT touching the hovered connector, so without this the plug-in drew
// the hover state it was never given — every cable stayed lit.
//
// Clears gHoverConnector first, so "over nothing" is as much an answer as "over this one".
//
// The pane UNDER THE CURSOR decides which Location to search — NOT gLocation, which follows the
// FOCUSED pane and so only changes on a click. Two things went wrong when this read gLocation:
// hovering the unfocused half searched the other half's modules, and because a module scrolled past
// its pane's foot still registers its connector rectangles (render_modules() needs them for cable
// geometry even when the module itself is clipped away), those rectangles land on screen inside the
// pane BELOW. Hovering the FX area therefore lit up Voice Area connectors sitting invisibly
// underneath it. Matching the pane fixes both: a connector can only be hit in the pane it was drawn
// in, where the scissor guarantees it is really visible.
//
// module_area_for_pane() is exactly the canvas, top bar and scrollbars already excluded, so the
// pane lookup subsumes the bounds check this used to make by hand. Returns -1 on the split bar.
void canvas_hover_update(tCoord coord) {
    gHoverConnector.active = false;

    int32_t  hoverPane = split_view_pane_at(coord);

    if (hoverPane < 0) {
        return;
    }
    uint32_t hoverSlot = gSlot;
    uint32_t hoverLoc  = split_view_location_for_pane((uint32_t)hoverPane);

    for (uint32_t idx = 0; idx < MAX_NUM_MODULES; idx++) {
        tModule * hoverModule = get_module_slot(hoverSlot, hoverLoc, idx);

        if (!hoverModule->active) {
            continue;
        }

        for (int i = 0; i < (int)module_connector_count(hoverModule->type); i++) {
            if (within_rectangle(coord, hoverModule->connector[i].rectangle)) {
                gHoverConnector.active      = true;
                gHoverConnector.slot        = hoverSlot;
                gHoverConnector.location    = hoverLoc;
                gHoverConnector.moduleIndex = hoverModule->key.index;
                gHoverConnector.ioCount     = (uint32_t)find_io_count_from_index(hoverModule, hoverModule->connector[i].dir, i);
                gHoverConnector.dir         = hoverModule->connector[i].dir;
                break;
            }
        }

        if (gHoverConnector.active) {
            break;
        }
    }
}

// ── Nudging the parameter under the pointer ─────────────────────────────────────────────────────
//
// Bare +/- steps the parameter under the pointer by one raw unit. Moved here from mouseHandle.c, where
// both of these were statics and therefore application-only: the plug-in had no keyboard at all, so
// the question never came up. It has one now, and this is the action behind it — the KEY DECODING
// stays in each shell, because a GLFW key code and an NSEvent's characters are not the same thing,
// and translating once at the boundary is the same split the modifier seam uses.
static bool nudge_one_param(tModule * module, uint32_t i, uint32_t variation, int delta) {
    tParam *   param = &module->param[variation][i];
    tParamType type  = paramTypeCommonDial;
    uint32_t   range = 128;
    int        newValue;

    if (module->key.location != locationMorph) {
        type  = paramLocationList[param->paramRef].type;
        range = paramLocationList[param->paramRef].range;
    }

    // Push is momentary and CustomData is not a scalar, so neither has a value to step. Return true
    // regardless: the pointer IS on this parameter, and answering false would let a widget behind it
    // take the keypress instead.
    if ((type == paramTypePush) || (type == paramTypeCustomData) || (range == 0)) {
        return true;
    }
    newValue = (int)param->value + delta;

    if (newValue < 0) {
        newValue = 0;
    } else if (newValue >= (int)range) {
        newValue = (int)range - 1;
    }

    if ((uint32_t)newValue != param->value) {
        param->value = (uint32_t)newValue;
        send_param_value(module->key.slot, module->key, i, variation, (uint32_t)newValue);
        send_param_value_to_links(module->key.slot, module->key, i, variation, (uint32_t)newValue);
    }
    return true;
}

// Mode selectors render as dials too, so pointing at one and getting nothing would be the surprise.
// They carry their own range table and their own write command.
static bool nudge_one_mode(tModule * module, uint32_t i, int delta) {
    tMode *  mode  = &module->mode[i];
    uint32_t range = modeLocationList[mode->modeRef].range;
    int      newValue;

    if (range == 0) {
        return true;
    }
    newValue = (int)mode->value + delta;

    if (newValue < 0) {
        newValue = 0;
    } else if (newValue >= (int)range) {
        newValue = (int)range - 1;
    }

    if ((uint32_t)newValue != mode->value) {
        mode->value = (uint32_t)newValue;
        send_mode_value(module->key.slot, module->key, i, (uint32_t)newValue);
    }
    return true;
}

// The FOCUSED parameter rather than the hovered one: the arrow keys act on what was last clicked,
// which is what the original editor does and what MIDI Learn already targets. Bare +/- keep acting on
// what is under the POINTER - two different questions, deliberately kept as two entry points.
bool canvas_nudge_focused_param(int delta) {
    if (!gParamFocus.valid) {
        return false;
    }
    tModule * module    = get_module(gParamFocus.moduleKey);

    if ((module == NULL) || !module->active) {
        // The module it pointed at has gone - deleted, or a different patch loaded underneath it.
        gParamFocus.valid = false;
        return false;
    }
    uint32_t  variation = gPatchDescr[gParamFocus.moduleKey.slot].activeVariation;

    return nudge_one_param(module, gParamFocus.paramIndex, variation, delta);
}

// Left/Right walk the focus along the module's own parameters, wrapping at each end. Manual p84: "To
// move the focus to another parameter in the module, press the Left/Right arrow buttons on the
// computer keyboard."
bool canvas_move_param_focus(int delta) {
    if (!gParamFocus.valid) {
        return false;
    }
    tModule * module = get_module(gParamFocus.moduleKey);

    if ((module == NULL) || !module->active) {
        gParamFocus.valid = false;
        return false;
    }
    uint32_t  count  = module_param_count(module->type);

    if (count == 0) {
        return false;
    }
    int32_t   next   = (int32_t)gParamFocus.paramIndex + delta;

    while (next < 0) {
        next += (int32_t)count;
    }
    gParamFocus.paramIndex = (uint32_t)(next % (int32_t)count);
    return true;
}

// Bring a module fully into view, scrolling the pane that shows its Location by the smallest amount
// that does it. Without this, Shift+arrows happily move the focus to a module that is scrolled off
// the canvas: the marks are drawn correctly, on something nobody can see.
//
// MINIMUM MOVEMENT, not centring - a keyboard walk down a column should creep the view along rather
// than jumping the focused module to the middle each step, which makes the surrounding modules leap
// about. A module already fully visible scrolls not at all.
static void scroll_module_into_view(tModule * module) {
    uint32_t   pane    = (module->key.location == (uint32_t)locationFx) ? 1 : 0;
    tRectangle area    = module_area_for_pane(pane);

    if (area.size.h <= 0.0) {
        return;   // that Location's pane is collapsed - nothing to reveal it into
    }
    // calc_scroll_x/y read the CURRENT pane, so ask as that pane and put it back. pane_scroll_by()
    // does the same dance internally for the write.
    uint32_t   prev    = module_pane();

    set_module_pane(pane);
    double     scrollX = calc_scroll_x();
    double     scrollY = calc_scroll_y();

    set_module_pane(prev);

    // The module's box in the same scaled pixels the scroll offsets are in - the terms
    // render_module() lays it out with, times the zoom.
    double     zoom    = get_zoom_factor();
    double     top     = module->row * MODULE_Y_SPAN * zoom;
    double     bottom  = top + (((gModuleProperties[module->type].height * MODULE_Y_SPAN) - MODULE_Y_GAP) * zoom);
    double     left    = module->column * MODULE_X_SPAN * zoom;
    double     right   = left + (MODULE_WIDTH * zoom);
    double     dx      = 0.0;
    double     dy      = 0.0;

    if (top < scrollY) {
        dy = top - scrollY;                            // above the band: pull the view up
    } else if (bottom > (scrollY + area.size.h)) {
        dy = bottom - (scrollY + area.size.h);         // below it: push the view down
    }

    if (left < scrollX) {
        dx = left - scrollX;
    } else if (right > (scrollX + area.size.w)) {
        dx = right - (scrollX + area.size.w);
    }

    if ((dx != 0.0) || (dy != 0.0)) {
        pane_scroll_by(pane, dx, dy);
    }
}

// Shift+arrows walk the focus from module to module. Manual p84: "To move the focus to another
// module in the Patch, press the Shift key on the computer keyboard together with the
// Up/Down/Left/Right arrow buttons. The modules in a Patch are accessed depending on how they were
// visually placed in the Patch window" - so this navigates by COLUMN AND ROW, not by module index.
// Index order is creation order, which after a few edits bears no relation to what is on screen.
//
// Up/Down stay in the column and take the nearest module above or below. Left/Right cross to the
// nearest column that HAS a module in that direction - skipping empty columns rather than stopping
// dead at one - and within it take the module whose row is closest to where the focus already was,
// which is what keeps a sideways move feeling horizontal.
bool canvas_move_module_focus(int dx, int dy) {
    if (!gParamFocus.valid) {
        return false;
    }
    tModule * from     = get_module(gParamFocus.moduleKey);

    if ((from == NULL) || !from->active) {
        gParamFocus.valid = false;
        return false;
    }
    uint32_t  slot     = gParamFocus.moduleKey.slot;
    uint32_t  location = gParamFocus.moduleKey.location;
    tModule * best     = NULL;
    uint32_t  bestKey  = 0;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * walk = get_module_slot(slot, location, i);

        if ((walk == NULL) || !walk->active || (walk->key.index == from->key.index)) {
            continue;
        }

        if (dy != 0) {
            if (walk->column != from->column) {
                continue;
            }

            // Nearest row strictly beyond the current one, in the direction asked for.
            if (  ((dy < 0) && (walk->row < from->row))
               || ((dy > 0) && (walk->row > from->row))) {
                uint32_t distance = (dy < 0) ? (from->row - walk->row) : (walk->row - from->row);

                if ((best == NULL) || (distance < bestKey)) {
                    best    = walk;
                    bestKey = distance;
                }
            }
        } else {
            if (  ((dx < 0) && (walk->column >= from->column))
               || ((dx > 0) && (walk->column <= from->column))) {
                continue;
            }
            // Column distance dominates so the nearest occupied column wins outright; row distance
            // only chooses between the modules sharing that column.
            uint32_t columnGap = (dx < 0) ? (from->column - walk->column) : (walk->column - from->column);
            uint32_t rowGap    = (walk->row > from->row) ? (walk->row - from->row) : (from->row - walk->row);
            uint32_t distance  = (columnGap * (MAX_ROWS + 1)) + rowGap;

            if ((best == NULL) || (distance < bestKey)) {
                best    = walk;
                bestKey = distance;
            }
        }
    }

    if (best == NULL) {
        return false;   // edge of the patch in that direction: leave the focus where it is
    }

    if (module_param_count(best->type) == 0) {
        return false;   // nothing on it the arrow keys could then act on
    }
    gParamFocus.valid      = true;
    gParamFocus.moduleKey  = best->key;
    gParamFocus.paramIndex = 0;
    scroll_module_into_view(best);
    return true;
}

bool canvas_nudge_param_under_cursor(int delta) {
    tCoord                coord     = {0};
    uint32_t              slot      = gSlot;
    uint32_t              variation = gPatchDescr[slot].activeVariation;

    get_global_gui_scaled_mouse_coord(&coord);

    // ONE QUERY. This used to walk every module in the Morph location and then every module in the
    // current one, testing each parameter's rectangle and then each mode's — morph first, because
    // morph knobs are drawn over the canvas and had to win a hit test against whatever sits beneath
    // them. The registry already knows that: the morph dials register at eClickLayerPanel and the
    // canvas widgets at eClickLayerCanvas, and the walk goes front to back. The old code's own
    // comment said as much — "the click path gets this ordering from the click-region layers
    // instead" — which is the duplication this removes rather than a difference to preserve.
    const tCanvasWidget * widget    = canvas_widget_at_any_layer(coord);

    // SCOPED, for the same reason canvas_right_click() is: the registry covers the whole screen, and
    // with both panes visible a widget belonging to the other one can be found under the pointer.
    // The walk this replaced was implicitly scoped — it iterated the Morph location and then
    // gLocation, and nothing else — so the scope has to be stated rather than assumed.
    if (  (widget != NULL)
       && (  (widget->key.slot != slot)
          || (  (widget->key.location != (uint32_t)locationMorph)
             && (widget->key.location != gLocation)))) {
        widget = NULL;
    }

    if (widget == NULL) {
        return false;
    }
    tModule *             module    = get_module(widget->key);

    if ((module == NULL) || !module->active) {
        return false;
    }

    switch (widget->kind) {
        case eCanvasWidgetParam:
        {
            // STEPPING A PARAMETER ALSO FOCUSES IT (CT, 2026-08-24), so the arrow keys carry on from
            // wherever +/- left off instead of from some older click - the two ways of nudging one
            // value stay on the same value.
            //
            // Only on a real step: delta 0 is the "is there anything here?" probe (mouseHandle.c's
            // alt-hover), and answering it must not move the focus. And only for a canvas parameter -
            // a morph dial never registers a param click either (see param_click_handler's note), and
            // a mode is not a parameter, so neither is something MIDI Learn could then act on.
            uint32_t index = canvas_widget_index(widget);
            bool     moved = nudge_one_param(module, index, variation, delta);

            if (moved && (delta != 0)) {
                gParamFocus.valid      = true;
                gParamFocus.moduleKey  = module->key;
                gParamFocus.paramIndex = index;
            }
            return moved;
        }

        case eCanvasWidgetMorph:
            return nudge_one_param(module, canvas_widget_index(widget), variation, delta);

        case eCanvasWidgetMode:
            return nudge_one_mode(module, canvas_widget_index(widget), delta);

        default:
            // A connector, a module body or its drag strip: nothing with a value to step. Answering
            // false lets the keypress go on to whatever else wants it.
            return false;
    }
}

// ── The gesture table ───────────────────────────────────────────────────────────────────────────
//
// See canvasDrag.h for why this exists. One row per gesture, one column per phase: a gesture whose
// release was never wired up is now a NULL sitting in plain sight rather than a phase that silently
// never runs, which is how the plug-in came to leave dials held and modules un-re-ordered.
//
// PRESS IS NOT A COLUMN HERE, and that is not an oversight. A press is a hit test, and the click-region
// registry already owns hit testing for the whole canvas (moduleGraphics.c registers every widget as it
// draws it); a press column would mean a second, competing answer to "what is under the pointer". What
// the press does have to do is call canvas_drag_begin(), and that is the one line each handler shares.

typedef struct {
    const char *   name;                                       // for a human reading a log or an assert
    tCanvasGesture id;
    bool (*motion)(const tCanvasGestureEvent * event);
    bool (*release)(const tCanvasGestureEvent * event);
} tCanvasGestureRow;

static bool param_gesture_motion(const tCanvasGestureEvent * event) {
    return canvas_param_drag_motion(event->coord, event->rawX, event->rawY, event->altHeld);
}

static bool module_gesture_motion(const tCanvasGestureEvent * event) {
    return module_drag_motion(event->coord);
}

static bool cable_gesture_motion(const tCanvasGestureEvent * event) {
    return cable_drag_motion(event->coord);
}

static bool rubber_band_gesture_motion(const tCanvasGestureEvent * event) {
    return rubber_band_motion(event->coord);
}

static bool param_gesture_release(const tCanvasGestureEvent * event) {
    (void)event;
    return canvas_param_drag_release();
}

static bool module_gesture_release(const tCanvasGestureEvent * event) {
    (void)event;
    return canvas_module_drag_release();
}

// CONNECT IF IT LANDED ON A CONNECTOR, AND CLEAR THE DRAG EITHER WAY. The clearing is the part that was
// diverging: handle_cable_connect() does not do it, so the plug-in memset gCableDrag by hand afterwards
// while the application relied on stop_dragging() doing it later. Both are now this.
static bool cable_gesture_release(const tCanvasGestureEvent * event) {
    if (gCableDrag.active == false) {
        return false;
    }
    (void)handle_cable_connect(event->coord, event->slot, event->location);
    memset(&gCableDrag, 0, sizeof(gCableDrag));
    return true;
}

static bool rubber_band_gesture_release(const tCanvasGestureEvent * event) {
    return canvas_rubber_band_release(event->coord, event->slot, event->location, event->additive);
}

// ORDER IS THE ORDER THEY ARE OFFERED THE EVENT. A press starts exactly one gesture, so in practice no
// two rows are ever active at once and the order is a tie-break that should never be needed — but it is
// written down once here instead of being implicit in two different shells, which is the point.
static const tCanvasGestureRow sGestures[] = {
    {"param",      canvasGestureParam,      param_gesture_motion,       param_gesture_release      },
    {"module",     canvasGestureModule,     module_gesture_motion,      module_gesture_release     },
    {"cable",      canvasGestureCable,      cable_gesture_motion,       cable_gesture_release      },
    {"rubberBand", canvasGestureRubberBand, rubber_band_gesture_motion, rubber_band_gesture_release},
};

tCanvasGesture canvas_gesture_motion(const tCanvasGestureEvent * event) {
    if (event == NULL) {
        return canvasGestureNone;
    }

    for (size_t i = 0; i < (sizeof(sGestures) / sizeof(sGestures[0])); i++) {
        if ((sGestures[i].motion != NULL) && sGestures[i].motion(event)) {
            return sGestures[i].id;
        }
    }

    return canvasGestureNone;
}

tCanvasGesture canvas_gesture_release(const tCanvasGestureEvent * event, tCanvasGesture wanted) {
    uint32_t acted = (uint32_t)canvasGestureNone;

    if (event == NULL) {
        return canvasGestureNone;
    }

    for (size_t i = 0; i < (sizeof(sGestures) / sizeof(sGestures[0])); i++) {
        if (((uint32_t)wanted & (uint32_t)sGestures[i].id) == 0) {
            continue;
        }

        if ((sGestures[i].release != NULL) && sGestures[i].release(event)) {
            acted |= (uint32_t)sGestures[i].id;
        }
    }

    return (tCanvasGesture)acted;
}
