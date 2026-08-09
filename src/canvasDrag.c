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

    if (gCableDrag.active == true) {
        cable_drag_set_end(coord);
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
    int  i     = 0;

    for (uint32_t idx = 0; idx < MAX_NUM_MODULES && !found; idx++) {
        tModule * module = get_module_slot(slot, location, idx);

        if (!module->active) {
            continue;
        }

        for (i = 0; i < (int)module_connector_count(module->type); i++) {
            if (within_rectangle(coord, module->connector[i].rectangle)) {
                open_connector_context_menu(coord, module->key, i);
                found = true;
                break;
            }
        }

        if (found == false) {
            uint32_t paramCount = module_param_count(module->type);

            for (uint32_t p = 0; p < paramCount && !found; p++) {
                if (within_rectangle(coord, gParamRectangle[module->key.slot][module->key.location][module->key.index][p])) {
                    open_param_context_menu(coord, module->key, p);
                    found = true;
                }
            }
        }

        if (found == false) {
            if (within_rectangle(coord, module->rectangle)) {
                open_module_context_menu(coord, module->key);
                found = true;
            }
        }
    }

    if (found == false) {
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
