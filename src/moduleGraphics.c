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

// Disable warnings from external library headers etc.
#include <math.h>
#include <stdint.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "utilsGraphics.h"
#include "waveModels.h"
#include "moduleGraphics.h"
#include "splitView.h"
#include "globalVars.h"
#include "renderParams.h"
#include "paramCurves.h"
#include "mouseHandle.h"
#include "menus.h"
#include "selection.h"
#include "mutatorUI.h"
#include "paramPages.h"
#include "paramOverlay.h"
#include "protocol.h"
#include "undo.h"
#include "canvasDrag.h"
#include "clickRegion.h"
#include "inputState.h"

// ── Click-region registration ────────────────────────────────────────────────
//
// Every clickable widget on the canvas — module params, mode toggles,
// connectors, the module body/drag-handle strip, and the morph group
// overlay — registers a clickable rect each frame right where its render
// function already computes it, instead of mouseHandle.c re-deriving
// hit-testing over every active module. Morph group dials register at
// eClickLayerPanel (a fixed on-screen overlay, unlike everything else here,
// which is eClickLayerCanvas and scrolls with the module area) — dispatch
// checks Panel before Canvas unconditionally, which is what lets a scrolled
// regular module sit visually underneath the morph overlay without stealing
// its clicks (see mouse_button()'s own comment on this).

// The leading {kind, key} pair is the shared prefix declared in moduleGraphics.h — see the comment
// there before reordering or adding a member.
typedef struct {
    eCanvasWidgetKind kind;
    tModuleKey        key;
    uint32_t          paramIndex;
} tParamClickCtx;

static tParamClickCtx     sParamClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_PARAMETERS];

// Where the Channel Select group being pressed was drawn. One press is in flight at a time, so one
// rectangle is enough — and it is only meaningful between this press and its own release, which is
// exactly the window the click registry's capture covers.
static tRectangle         sRadioPressRect;

typedef struct {
    eCanvasWidgetKind kind;
    tModuleKey        key;
} tModuleClickCtx;

static tModuleClickCtx    sModuleClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES];

typedef struct {
    eCanvasWidgetKind kind;
    tModuleKey        key;
    uint32_t          modeIndex;
} tModeClickCtx;

static tModeClickCtx      sModeClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_MODES];

typedef struct {
    eCanvasWidgetKind kind;
    tModuleKey        key;
    uint32_t          connectorIndex;
} tConnectorClickCtx;

static tConnectorClickCtx sConnectorClickCtx[MAX_SLOTS][locationMax][MAX_NUM_MODULES][MAX_NUM_CONNECTORS];

// Mirrors the params loop previously in mouseHandle.c's
// handle_module_press_for_module()/handle_module_release_for_module() — see
// git history for that code. Only reachable for non-morph modules (this
// handler is only ever registered from render_param_common(), which morph
// groups don't go through), so the paramType-by-location branch those
// functions needed is gone here.
static void param_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    tParamClickCtx * ctx       = (tParamClickCtx *)userData;
    tModule *        module    = get_module(ctx->key);
    uint32_t         slot      = ctx->key.slot;
    uint32_t         variation = gPatchDescr[slot].activeVariation;
    tParam *         param     = &module->param[variation][ctx->paramIndex];
    tParamType       paramType = paramLocationList[param->paramRef].type;

    if (phase == eClickPress) {
        // The parameter MIDI Learn will act on. Every param type, not just the draggable ones — a
        // button can carry a CC too. This has to live here rather than in mouseHandle.c's
        // handle_module_press_for_module(), which looks like the click path but is legacy fallback:
        // every widget registers a click region at render time and dispatch_click_region() gets
        // there first, so that function no longer runs for a canvas parameter.
        gParamFocus.valid      = true;
        gParamFocus.moduleKey  = module->key;
        gParamFocus.paramIndex = ctx->paramIndex;
        LOG_INFO("Param focus: slot %u location %u module %u param %u (type %u)\n",
                 module->key.slot, module->key.location, module->key.index, ctx->paramIndex, paramType);

        if (  paramType != paramTypeToggle && paramType != paramTypeMenu
           && paramType != paramTypeBypass && paramType != paramTypeEnable
           && paramType != paramTypePush && paramType != paramTypeCustomData
           && paramType != paramTypeRadioEdit) {
            gParamDragging.moduleKey       = module->key;
            gParamDragging.type3           = paramType3Param;
            gParamDragging.param           = ctx->paramIndex;
            gParamDragging.startValue      = param->value;
            gParamDragging.active          = true;
            // From the registry's own capture, which dispatch armed with this region before calling
            // this handler — so the drag holds the exact rectangle that was clicked, rather than
            // looking the same thing up again in an array that a later frame may have rewritten.
            // See click_region_capture_rect() and tParamDragging.
            click_region_capture_rect(&gParamDragging.rect);
            gParamDragging.startMorphRange = param->morphRange[gMorphGroupFocus];

            if ((synthlib_dial_mode() != eDialModeRotary) || (paramType == paramTypeSlider)) {
                canvas_drag_begin();
            }
        } else if (paramType == paramTypeRadioEdit) {
            // THE GROUP'S RECTANGLE, TAKEN NOW. click_region_capture_rect() answers only while a
            // press is in flight — dispatch_click_region() clears the capture BEFORE it calls the
            // release handler, deliberately, so a handler that re-enters dispatch cannot find a
            // stale one. Asking for it on release returns false and leaves the rect zeroed, and the
            // hit test then finds no button under any coordinate at all.
            sRadioPressRect = (tRectangle){
                0
            };
            click_region_capture_rect(&sRadioPressRect);
        } else if (paramType == paramTypePush) {
            // A push is MOMENTARY, and it fires on the way DOWN — the mouse button being held is what
            // gives the pulse its width. This used to be the other way round: 0 on press and 1 on
            // release, which left the device holding the parameter at 1 for good. That value is
            // stored in the patch, so the G2 acted on it again every time it recompiled.
            //
            // SeqVal's "Rnd" is where that bit: press it once, then add or delete ANY module, and the
            // instrument re-randomised all 16 steps and reported the new values back — which is the
            // "sequencer data updates randomly when I add a module" report. The editor was showing
            // the truth; the latched button was the cause. Confirmed both ways on hardware: clearing
            // param 36 by hand stopped it dead, and setting it back to 1 randomised the sequence
            // within two seconds without any other traffic.
            //
            // The 0 has to be a separate event rather than the next line down: sent back-to-back in
            // the same millisecond, the device sees the release before it acts on the trigger and
            // nothing happens at all.
            uint32_t listSize = array_size_param_location_list();

            for (uint32_t ref = 0; ref < listSize; ref++) {
                if ((paramLocationList[ref].moduleType == module->type) && (paramLocationList[ref].type == paramTypeCustomData)) {
                    send_custom_data_value(slot, module->key);
                    break;
                }
            }

            send_param_value(slot, module->key, ctx->paramIndex, variation, 1);
            param->value = 0;  // Momentary: there is no pressed state to draw
        }
    } else if (phase == eClickRelease) {
        if ((paramType == paramTypeMenu) || (paramType == paramTypeCustomData)) {
            open_toggle_menu(coord, module->key, ctx->paramIndex, param->paramRef);
        } else if (paramType == paramTypeRadioEdit) {
            // Radio: the value IS the button, so there is no cycling — whichever box the cursor is
            // over becomes the selection. One click region covers the whole group and the button
            // falls out of the geometry, which keeps the group a single widget everywhere else in
            // the app (hit tests, knob assignment, the parameter pages) and needs no second table.
            uint32_t range       = paramLocationList[param->paramRef].range;
            uint32_t oldParamVal = param->value;
            int32_t  button      = radio_button_at(sRadioPressRect, range, coord);

            if ((button < 0) || ((uint32_t)button == oldParamVal)) {
                return;
            }
            param->value = (uint32_t)button;
            send_param_value(slot, module->key, ctx->paramIndex, variation, param->value);
            undo_push_param_change(module->key, ctx->paramIndex, variation, oldParamVal, param->value);
            send_param_value_to_links(slot, module->key, ctx->paramIndex, variation, param->value);
        } else if ((paramType == paramTypeToggle) || (paramType == paramTypeBypass) || (paramType == paramTypeEnable)) {
            uint32_t range       = paramLocationList[param->paramRef].range;
            uint32_t oldParamVal = param->value;

            param->value = (param->value + 1) % range;
            send_param_value(slot, module->key, ctx->paramIndex, variation, param->value);
            undo_push_param_change(module->key, ctx->paramIndex, variation, oldParamVal, param->value);
            send_param_value_to_links(slot, module->key, ctx->paramIndex, variation, param->value);
        } else if (paramType == paramTypePush) {
            // The button coming back up. The trigger itself went out on press — see the comment there.
            send_param_value(slot, module->key, ctx->paramIndex, variation, 0);
            param->value = 0;
        }
    }
}

// Mirrors the modes loop previously in mouseHandle.c's
// handle_module_press_for_module()/handle_module_release_for_module() — see
// git history for that code. Modes only ever have two release-relevant
// types (Menu, Toggle); anything else (e.g. paramTypeOscWave) is a plain
// drag, armed on press.
static void mode_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    tModeClickCtx * ctx      = (tModeClickCtx *)userData;
    tModule *       module   = get_module(ctx->key);
    tMode *         mode     = &module->mode[ctx->modeIndex];
    tParamType      modeType = modeLocationList[mode->modeRef].type;

    if (phase == eClickPress) {
        if ((modeType != paramTypeToggle) && (modeType != paramTypeMenu)) {
            memset(&gParamDragging, 0, sizeof(gParamDragging));
            gParamDragging.moduleKey  = module->key;
            gParamDragging.type3      = paramType3Mode;
            gParamDragging.mode       = ctx->modeIndex;
            gParamDragging.startValue = mode->value;
            gParamDragging.active     = true;

            if (synthlib_dial_mode() != eDialModeRotary) {
                canvas_drag_begin();
            }
        }
    } else if (phase == eClickRelease) {
        if (modeType == paramTypeMenu) {
            open_mode_toggle_menu(coord, module->key, ctx->modeIndex, mode->modeRef);
        } else if (modeType == paramTypeToggle) {
            uint32_t oldModeVal = mode->value;

            mode->value = (mode->value + 1) % modeLocationList[mode->modeRef].range;
            send_mode_value(ctx->key.slot, module->key, ctx->modeIndex, mode->value);
            undo_push_mode_change(module->key, ctx->modeIndex, oldModeVal, mode->value);
        }
    }
}

// Mirrors the connectors loop previously in mouseHandle.c's
// handle_module_press_for_module() — see git history for that code. Press
// only: connector release (completing a cable) is handled entirely
// separately, by handle_cable_connect() re-scanning every connector against
// the release coord directly — it doesn't care which connector (if any) was
// originally pressed, so it isn't a per-widget dispatch target.
static void connector_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    if (phase != eClickPress) {
        return;
    }
    tConnectorClickCtx * ctx    = (tConnectorClickCtx *)userData;
    tModule *            module = get_module(ctx->key);

    gCableDrag.rerouting = false;

    // CTRL-CLICK PICKS UP THE CABLE THAT IS ALREADY THERE instead of starting a new one, and drags
    // its FAR end — so the end under the cursor is the one that moves, which is what "pull out the
    // connector" means. Everything after this point is the ordinary drag: the release either lands
    // on a connector, and the cable is re-routed, or it does not, and the cable is simply gone.
    // Manual p65. With no cable on the connector there is nothing to pick up, so it falls through
    // and draws a new one as an unmodified click would.
    if (ctrl_modifier_held()) {
        tConnectorDir dir              = module->connector[ctx->connectorIndex].dir;
        int           ioCount          = find_io_count_from_index(module, dir, (int)ctx->connectorIndex);
        tCableKey     key              = {0};
        uint32_t      otherModuleIndex = 0;
        uint32_t      otherIoCount     = 0;
        tConnectorDir otherDir         = connectorDirIn;

        // Finding ONE cable here is only about where to draw the rubber band from — the move itself
        // takes every cable on this hole (handle_cable_reroute()), which is why what gets recorded is
        // the hole and not that cable.
        if (  (ioCount >= 0)
           && find_cable_at_connector(module->key.slot, module->key.location, module->key.index,
                                      (uint32_t)ioCount, dir, &key, &otherModuleIndex, &otherIoCount, &otherDir)) {
            tModule * otherModule = get_module_slot(module->key.slot, module->key.location, otherModuleIndex);
            int       otherIndex  = (otherModule != NULL) ? find_index_from_io_count(otherModule, otherDir, (int)otherIoCount) : -1;

            if (otherIndex >= 0) {
                gCableDrag.rerouting          = true;
                gCableDrag.rerouteModuleIndex = module->key.index;
                gCableDrag.rerouteIoCount     = (uint32_t)ioCount;
                gCableDrag.rerouteDir         = dir;
                gCableDrag.fromModuleKey      = otherModule->key;
                gCableDrag.fromConnectorIndex = (uint32_t)otherIndex;
                cable_drag_set_end(coord);
                gCableDrag.active             = true;
                return;
            }
        }
    }
    gCableDrag.fromModuleKey      = module->key;
    gCableDrag.fromConnectorIndex = ctx->connectorIndex;
    cable_drag_set_end(coord);   // same placement the motion uses, so the end doesn't jump on the first move
    gCableDrag.active             = true;
}

// Mirrors the final "clicking anywhere else on the module body selects
// without starting a drag" fallback previously in mouseHandle.c's
// handle_module_press_for_module() — see git history for that code.
static void module_body_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;

    if (phase != eClickPress) {
        return;
    }
    tModuleClickCtx * ctx             = (tModuleClickCtx *)userData;
    tModule *         module          = get_module(ctx->key);
    bool              multiSelectHeld = multi_select_modifier_held();

    if (multiSelectHeld) {
        selection_toggle(module->key);
    } else {
        selection_set_single(module->key);
    }
}

// Mirrors the "module->dragArea" branch previously in mouseHandle.c's
// handle_module_press_for_module() — see git history for that code. Registered
// on module->dragArea *after* module_body_click_handler is registered on the
// (larger, overlapping) module->rectangle, so this wins for clicks landing in
// the drag-handle strip, exactly like the old first-match-wins loop order.
static void drag_area_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;

    if (phase != eClickPress) {
        return;
    }
    tModuleClickCtx * ctx             = (tModuleClickCtx *)userData;
    tModule *         module          = get_module(ctx->key);
    bool              multiSelectHeld = multi_select_modifier_held();

    if (multiSelectHeld) {
        selection_toggle(module->key);
    } else if (!is_selected(module->key)) {
        selection_set_single(module->key);
    }
    gModuleDrag.moduleKey     = module->key;
    gModuleDrag.isMulti       = is_selected(module->key) && gSelection.count > 1;
    gModuleDrag.prevColumn    = module->column;
    gModuleDrag.prevRow       = module->row;
    gModuleDrag.active        = true;
    gModuleDrag.snapshotCount = 0;

    if (gModuleDrag.isMulti) {
        for (uint32_t si = 0; si < gSelection.count && si < MAX_NUM_MODULES; si++) {
            tModule * sel = get_module(gSelection.keys[si]);

            if (!sel) {
                continue;
            }
            gModuleDrag.snapshotKeys[gModuleDrag.snapshotCount]   = gSelection.keys[si];
            gModuleDrag.snapshotColumn[gModuleDrag.snapshotCount] = sel->column;
            gModuleDrag.snapshotRow[gModuleDrag.snapshotCount]    = sel->row;
            gModuleDrag.snapshotCount++;
        }
    } else {
        gModuleDrag.snapshotKeys[0]   = module->key;
        gModuleDrag.snapshotColumn[0] = module->column;
        gModuleDrag.snapshotRow[0]    = module->row;
        gModuleDrag.snapshotCount     = 1;
    }
}

// Mirrors the morph-specific branch of the params loop previously in
// mouseHandle.c's handle_module_press_for_module()/
// handle_module_release_for_module() (location == locationMorph) — see git
// history for that code. Registered at eClickLayerPanel by
// render_morph_groups() below, not eClickLayerCanvas like every other
// handler in this file — see this file's own top-of-file comment for why.
// userData carries the param index (0..NUM_MORPHS*2-1) as a plain integer,
// not a pointer — the morph module is a fixed singleton ({gSlot,
// locationMorph, 1}), so there's no per-instance context to point to the way
// a regular module's tModuleKey needs.
//
// Unlike a regular module param, morph's own paramType is derived purely
// from which half of the index range i falls in (i < NUM_MORPHS = the dial
// itself, always paramTypeCommonDial; i >= NUM_MORPHS = the knob/morph-name
// label underneath it, always paramTypeToggle) — never from
// paramLocationList[param->paramRef].type the way param_click_handler reads
// it. That collapses the original 3-way paramType branch down to: the dial
// half only ever arms a drag (on press), the label half only ever toggles
// (on release, flipping the isKnob flag render_morph_groups() reads).
// A TAGGED CONTEXT, NOT AN INTEGER CAST TO A POINTER. These regions used to carry (void *)(intptr_t)i
// as their user data, which works for a handler that knows what it registered and is a landmine for
// anything that asks the registry what is under the cursor: reading a tag off it dereferences a small
// integer. The morph module's own slots in sParamClickCtx are free — render_param_common() is never
// called for a morph module, which is what fills them for everything else — so the context lives
// there, tagged eCanvasWidgetMorph.
static void * morph_click_ctx(const tModule * module, uint32_t index) {
    tParamClickCtx * ctx = &sParamClickCtx[module->key.slot][module->key.location][module->key.index][index];

    *ctx = (tParamClickCtx){
        eCanvasWidgetMorph, module->key, index
    };

    return ctx;
}

// Defined further down, beside the wave models they draw from.
static double module_wave_sample(uint32_t moduleType, uint32_t waveValue, double phase, double shape);
bool module_wave_picker_mode(uint32_t moduleType, uint32_t modeIndex);

static void morph_param_click_handler(tCoord coord, eClickPhase phase, void * userData) {
    (void)coord;
    uint32_t  i         = ((const tParamClickCtx *)userData)->paramIndex;
    tModule * module    = get_module((tModuleKey){gSlot, (uint32_t)locationMorph, 1});
    uint32_t  variation = gPatchDescr[gSlot].activeVariation;
    tParam *  param     = &module->param[variation][i];

    if (phase == eClickPress) {
        if (i < NUM_MORPHS) {
            gParamDragging.moduleKey       = module->key;
            gParamDragging.type3           = paramType3Param;
            gParamDragging.param           = i;
            gParamDragging.startValue      = param->value;
            gParamDragging.active          = true;
            gMorphGroupFocus               = i;
            gParamDragging.startMorphRange = param->morphRange[gMorphGroupFocus];

            if (synthlib_dial_mode() != eDialModeRotary) {
                canvas_drag_begin();
            }
        }
    } else if (phase == eClickRelease) {
        if (i >= NUM_MORPHS) {
            uint32_t oldParamVal = param->value;

            param->value = (param->value + 1) % 2;
            send_param_value(gSlot, module->key, i, variation, param->value);
            undo_push_param_change(module->key, i, variation, oldParamVal, param->value);
            send_param_value_to_links(gSlot, module->key, i, variation, param->value);
        }
    }
}

void render_volume_meter(tRectangle rectangle, tVolumeType volumeType, uint32_t value) { // TODO: move to utilsgraphics!?
    const tVolumeMeterConfig * config = find_volume_meter_config(volumeType);

    if (config == NULL) {
        return;
    }

    switch (config->style) {
        case volumeMeterStyleMaskLeds:
        {
            tRectangle smallRectangle = rectangle;
            double     space          = config->space;
            uint32_t   leds           = config->segments;

            smallRectangle.coord.y += space;
            smallRectangle.coord.x += space;
            smallRectangle.size.h   = (smallRectangle.size.h - (space * (double)(leds + 1))) / (double)leds;
            smallRectangle.size.w  -= space * 2;

            set_rgb_colour((tRgb)RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            value                  &= 0x0ff;                         // There's a value of 3 in the high nibble, which is unknown use. Might be an indication of this being individual bit per LED?

            for (int i = 0; i < leds; i++) {
                if ((value >> i) & 0x01) {
                    set_rgb_colour(config->onColour);
                } else {
                    set_rgb_colour(config->offColour);
                }
                render_rectangle(moduleArea, smallRectangle);
                smallRectangle.coord.y += smallRectangle.size.h + space;
            }

            break;
        }

        case volumeMeterStyleLevelBar:
        {
            uint32_t level             = value & 0x0f;
            //bool     yellowHold        = ((value >> 4) & 0x03) != 0;
            //bool     hold        = ((value >> 5) & 0x01) != 0;
            bool     clip              = ((value >> 6) & 0x01) != 0;

            double   fullHeight        = rectangle.size.h;
            double   stepHeight        = fullHeight / (double)config->segments;
            int      valueThresholds[] = {7, 11, 12}; // exclusive upper bounds: green/yellow/red
            tRgb     colours[]         = {RGB_GREEN_7, (tRgb)RGB_YELLOW_7, RGB_RED_7};

            set_rgb_colour((tRgb)RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            double   previousHeight    = 0;

            for (int i = 0; i < 3; i++) {
                int    segmentTopVal     = valueThresholds[i];
                int    segmentBottomVal  = (i == 0) ? 0 : valueThresholds[i - 1];
                int    segmentRange      = segmentTopVal - segmentBottomVal;
                double segmentDrawHeight = 0;

                if ((int)level >= segmentBottomVal) {
                    int drawSteps = ((int)level < segmentTopVal) ? (int)level - segmentBottomVal : segmentRange;
                    segmentDrawHeight = (drawSteps * fullHeight) / (double)config->segments;

                    set_rgb_colour(colours[i]);
                    render_rectangle(
                        moduleArea,
                        (tRectangle){{rectangle.coord.x,
                                      rectangle.coord.y + fullHeight - previousHeight - segmentDrawHeight},
                                     {rectangle.size.w,
                                      segmentDrawHeight}
                        });
                    previousHeight   += segmentDrawHeight;
                }
            }

            // Clip: bright red stripe at the very top of the meter (red zone) when clipping.
            if (clip) {
                set_rgb_colour((tRgb)RGB_RED_7);
                render_rectangle(moduleArea,
                                 (tRectangle){{rectangle.coord.x, rectangle.coord.y},
                                              {rectangle.size.w, stepHeight}
                                 });
            }
            break;
        }

        case volumeMeterStyleSingleLed:
        {
            tRectangle smallRectangle = rectangle;
            double     space          = config->space;
            uint32_t   leds           = config->segments;

            smallRectangle.coord.y += space;
            smallRectangle.coord.x += space;
            smallRectangle.size.w   = (smallRectangle.size.w - (space * (double)(leds + 1))) / (double)leds;
            smallRectangle.size.h  -= space * 2;

            set_rgb_colour((tRgb)RGB_BLACK);
            render_rectangle(moduleArea, rectangle);

            value                  &= 0x0ff;                         // There's a value of 3 in the high nibble, which is unknown use. Might be an indication of this being individual bit per LED?

            for (int i = 0; i < leds; i++) {
                if (i == value) {
                    set_rgb_colour(config->onColour);
                } else {
                    set_rgb_colour(config->offColour);
                }
                render_rectangle(moduleArea, smallRectangle);
                smallRectangle.coord.x += smallRectangle.size.w + space;
            }

            break;
        }
    }
}

// This might be too generic and won't be able to use, or we add extra params!
// TODO: possibly move all the type cases into functions in a new source file, references by function pointer?
// The focused parameter - the one the arrow keys would act on, and the one MIDI Learn targets.
// Marked with four corner brackets rather than a full outline, because a full one is already taken:
// a SELECTED MODULE draws a complete yellow box (see below), and a second complete box inside it
// would read as the same idea at a smaller size. Corners are also the only frame that fits a widget
// whose rectangle is mostly text - a dial's rect carries its label and value, so a full border would
// box up words that are not part of the control.
static void render_param_focus_marks(tRectangle rect) {
    double x         = rect.coord.x;
    double y         = rect.coord.y;
    double w         = rect.size.w;
    double h         = rect.size.h;
    // BOTH SIZES COME FROM THE RECT, and that is what makes them zoom. widgetRect is already in
    // screen space, so it grows with the canvas zoom; anything derived from it grows with it, while
    // a constant does not. An earlier version capped the arm at 3.5 and set the thickness to a flat
    // 1.5 - the cap was already biting at 100% zoom (a dial rect is about 24.5 across, so 0.175 of
    // it is 4.3), which left the marks a fixed size while the dial they bracket grew underneath.
    //
    // Short on purpose: a third of the side crowds the dial, so this is half the length that first
    // looked right.
    double len       = fmin(w, h) * 0.175;
    double thickness = fmax(fmin(w, h) * 0.06, 1.0);

    if ((w <= 0.0) || (h <= 0.0) || (len < 1.0)) {
        return;
    }
    // mainArea, NOT moduleArea, and that is the whole trick. render_line() applies the canvas zoom
    // and scroll for moduleArea - but widgetRect has ALREADY had them applied: it is the rectangle
    // handed to register_click_region() and hit-tested against raw mouse coordinates, so it is in
    // screen space before it gets here. Passing moduleArea transforms it a second time and the marks
    // land off-canvas, drawn perfectly and nowhere near the dial.
    set_rgb_colour((tRgb)RGB_BLACK);
    render_line(mainArea, (tCoord){x, y}, (tCoord){x + len, y}, thickness);                          // top left
    render_line(mainArea, (tCoord){x, y}, (tCoord){x, y + len}, thickness);
    render_line(mainArea, (tCoord){x + w, y}, (tCoord){x + w - len, y}, thickness);                  // top right
    render_line(mainArea, (tCoord){x + w, y}, (tCoord){x + w, y + len}, thickness);
    render_line(mainArea, (tCoord){x + w, y + h}, (tCoord){x + w - len, y + h}, thickness);          // bottom right
    render_line(mainArea, (tCoord){x + w, y + h}, (tCoord){x + w, y + h - len}, thickness);
    render_line(mainArea, (tCoord){x, y + h}, (tCoord){x + len, y + h}, thickness);                  // bottom left
    render_line(mainArea, (tCoord){x, y + h}, (tCoord){x, y + h - len}, thickness);
}

tRectangle render_param_common(tRectangle rectangle, tModule * module, uint32_t paramRef, uint32_t paramIndex) {
    // WHERE THE WIDGET ACTUALLY WENT. A local, because that is all it ever was: the value is written
    // here, handed to register_click_region() a few lines down and returned to the caller, and
    // nothing reads it afterwards. It used to be a slot in a 6MB [slot][location][module][param]
    // global that every hit test in the app then re-read — see the migration note in Docs/todo.txt.
    tRectangle widgetRect                  = {0};
    char       buff[16]                    = {0};
    char       label[CLAVIA_NAME_SIZE + 1] = {0};
    // The module's own Slot, not gSlot: identical while rendering the canvas (which only ever
    // draws the selected Slot), but the Parameter Pages panel reuses these widgets to draw a
    // Global page's knobs, and those can point at a module in any of the four Slots - each with
    // its own active Variation. Same reason the renderParams.c widgets read module->key.slot.
    uint32_t   slot                        = module->key.slot;
    uint32_t   variation                   = gPatchDescr[slot].activeVariation;
    uint32_t   paramValue                  = module->param[variation][paramIndex].value;
    uint32_t   morphRange                  = module->param[variation][paramIndex].morphRange[gMorphGroupFocus];

    if (paramValue >= paramLocationList[paramRef].range) {
        LOG_ERROR("Module index %u name %s ParamRef %u ParamIndex %u Value %u > Range %u\n", module->key.index, module->name, paramRef, paramIndex, paramValue, paramLocationList[paramRef].range);
        paramValue = 0;  // If we hit this, the module config needs fixing, but letting it through for now
    }

    // A renamed parameter is drawn under its new name — EXCEPT a Channel Select group, where name 0
    // is the first BUTTON's caption, not a name for the group. Taking it as the group label drew
    // "Box1" both above the group and on the button.
    if ((strlen(module->paramName[paramIndex][0]) > 0) && (paramLocationList[paramRef].type != paramTypeRadioEdit)) {
        COPY_STRING(label, module->paramName[paramIndex][0]);
    } else if (paramLocationList[paramRef].label != NULL) {
        COPY_STRING(label, paramLocationList[paramRef].label);
    }
    label[sizeof(label) - 1]                      = '\0';

    module->param[variation][paramIndex].paramRef = paramRef;

    //LOG_DEBUG("param %u\n", paramValue);

    switch (paramLocationList[paramRef].type) {
        case paramTypeCustomData:
        case paramTypeToggle:
        case paramTypeMenu:
        {
            // A WAVEFORM PICKER IS STILL AN ORDINARY paramTypeMenu — only its FACE differs. It was
            // briefly given a param type of its own, and that broke the drop-down: the type is read
            // in a dozen places (canvasDrag, paramPages, mutator, the click routing below) and every
            // one of them has to learn a new value. Deciding here instead leaves all of that code
            // seeing exactly the menu it saw before, so dragging, the popup and the name list keep
            // working with no changes anywhere else.
            static const char * blankCaptions[] = {
                WAVE_MENU_CAPTION, WAVE_MENU_CAPTION, WAVE_MENU_CAPTION,
                WAVE_MENU_CAPTION, WAVE_MENU_CAPTION, WAVE_MENU_CAPTION,
                WAVE_MENU_CAPTION, WAVE_MENU_CAPTION, NULL
            };
            bool                isWavePicker    = module_wave_picker_param(module->type, paramIndex);
            const char **       captions        = isWavePicker ? blankCaptions : paramLocationList[paramRef].strMap;

            widgetRect = render_paramType1StandardToggle(module, rectangle, label, buff, sizeof(buff), paramValue,
                                                         paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5,
                                                         paramIndex, paramRef, captions);

            if (isWavePicker == true) {
                // Drawn at a FIXED shape, not the module's current one: measured 2026-08-23, all four
                // of the shape oscillators' sines are identical pure sines at Shape 0, so a live icon
                // would draw the same picture for every entry there. Three quarters develops every
                // oscillator wave without going so far that SymPulse falls silent (it does, at the
                // top of its dial) or Pulse narrows to a sliver; LfoShpA's dial is bipolar with its
                // neutral wave at the CENTRE, so it takes 0.5, which is also where its Sqr2Tri reads
                // as a trapezoid rather than a second square beside Sqr.
                render_wave_icon(widgetRect, module->type, (uint32_t)paramValue,
                                 module_wave_icon_shape(module->type));
            }
            break;
        }
        case paramTypeRadioEdit:
        {
            widgetRect = render_paramType1RadioEdit(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            break;
        }
        case paramTypeBypass:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
            render_param_function = &render_paramType1Bypass;

            if (render_param_function != NULL) {
                widgetRect = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            }
            break;
        }
        case paramTypeEnable:
        case paramTypePush:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramIndex, uint32_t paramRef, const char ** strMap);
            render_param_function = &render_paramType1Enable;

            if (render_param_function != NULL) {
                widgetRect = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramIndex, paramRef, paramLocationList[paramRef].strMap);
            }
            break;
        }
        default:
        {
            tRectangle (*render_param_function)(tModule * module, tRectangle rectangle, char * label, char * buff, int buffSize, double paramValue, uint32_t range, uint32_t morphrange, tRgb colour, uint32_t paramRef);
            render_param_function = NULL;

            switch (paramLocationList[paramRef].type) {
                case paramTypeFreq:           render_param_function      = &render_paramType1Freq;
                    break;
                case paramTypeOscFreq:        render_param_function      = &render_paramType1OscFreq;
                    break;
                case paramTypeFine:           render_param_function      = &render_paramType1Fine;
                    break;
                case paramTypeGeneralFreq:    render_param_function      = &render_paramType1GeneralFreq;
                    break;
                case paramTypeShape:          render_param_function      = &render_paramType1Shape;
                    break;
                case paramTypeLfoShape:       render_param_function      = &render_paramType1LfoShape;
                    break;
                case paramTypeFreqDrum:       render_param_function      = &render_paramType1FreqDrum;
                    break;
                case paramTypeLFORate:        render_param_function      = &render_paramType1LFORate;
                    break;
                case paramTypeInt:            render_param_function      = &render_paramType1Int;
                    break;
                case paramTypedB:             render_param_function      = &render_paramType1dB;
                    break;
                case paramTypeMixLevel:       render_param_function      = &render_paramType1MixLevel;
                    break;
                case paramTypeTime:           render_param_function      = &render_paramType1Time;
                    break;
                case paramTypeTimeClk:        render_param_function      = &render_paramType1TimeClk;
                    break;
                case paramTypeADRTime:        render_param_function      = &render_paramType1ADRTime;
                    break;
                case paramTypePulseTime:      render_param_function      = &render_paramType1PulseTime;
                    break;
                case paramTypePitch:          render_param_function      = &render_paramType1Pitch;
                    break;
                case paramTypeBipLevel:       render_param_function      = &render_paramType1BipLevel;
                    break;
                case paramTypePartials:       render_param_function      = &render_paramType1Partials;
                    break;
                case paramTypeUniPol:         render_param_function      = &render_paramType1UniPol;
                    break;
                case paramTypeLevAmpDial:     render_param_function      = &render_paramType1LevAmpDial;
                    break;
                case paramTypeResonanceQ:         render_param_function  = &render_paramType1ResonanceQ;
                    break;

                case paramTypeFlangerRate:        render_param_function  = &render_paramType1FlangerRate;
                    break;

                case paramTypePhaserRate:         render_param_function  = &render_paramType1PhaserRate;
                    break;

                case paramTypeSwing:          render_param_function      = &render_paramType1Swing;
                    break;

                case paramTypeBandwidth:      render_param_function      = &render_paramType1Bandwidth;
                    break;

                case paramTypePhase:          render_param_function      = &render_paramType1Phase;
                    break;

                case paramTypePan:            render_param_function      = &render_paramType1Pan;
                    break;
                case paramTypeNoteDial:       render_param_function      = &render_paramType1NoteDial;
                    break;
                case paramTypePShiftSemi:          render_param_function = &render_paramType1PShiftSemi;
                    break;

                case paramTypeBipolarPinned:       render_param_function = &render_paramType1BipolarPinned;
                    break;

                case paramTypePlusMinusUnits:      render_param_function = &render_paramType1PlusMinusUnits;
                    break;

                case paramTypeOffNum:              render_param_function = &render_paramType1OffNum;
                    break;

                case paramTypeScratchRatio:        render_param_function = &render_paramType1ScratchRatio;
                    break;

                case paramTypeSampleRate:          render_param_function = &render_paramType1SampleRate;
                    break;

                case paramTypeThresholdDb:         render_param_function = &render_paramType1ThresholdDb;
                    break;

                case paramTypeBipolar:        // -64..+63, zero at the centre
                case paramTypeLRDial:         // pan dial, same bipolar reading
                    render_param_function                                = &render_paramType1Bipolar;
                    break;

                case paramTypeCommonDial:     // default percent dial
                case paramTypeResonance:      render_param_function      = &render_paramType1Resonance;
                    break;
                case paramTypeSlider:         render_param_function      = &render_paramType1Slider;
                    break;
                case paramTypeStrMap:         render_param_function      = &render_paramType1StrMap;
                    break;
                case paramTypeFreqShift:      render_param_function      = &render_paramType1FreqShift;
                    break;
                default:                      LOG_ERROR("Unrecognised paramType %d\n", paramLocationList[paramRef].type);
                    break;
            }

            if (render_param_function != NULL) {
                widgetRect = render_param_function(module, rectangle, label, buff, sizeof(buff), paramValue, paramLocationList[paramRef].range, morphRange, (tRgb)RGB_GREY_5, paramRef);
            }
            break;
        }
    }
    sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex] = (tParamClickCtx){
        eCanvasWidgetParam, module->key, paramIndex
    };
    register_click_region(widgetRect, eClickLayerCanvas, param_click_handler,
                          &sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex]);
    param_overlay_note_param(module, paramIndex, rectangle, buff);

    if (  gParamFocus.valid
       && (gParamFocus.moduleKey.slot == module->key.slot)
       && (gParamFocus.moduleKey.location == module->key.location)
       && (gParamFocus.moduleKey.index == module->key.index)
       && (gParamFocus.paramIndex == paramIndex)) {
        render_param_focus_marks(widgetRect);
    }
    // Hand back where the widget actually went, for a caller that needs it for its own hit-testing.
    return widgetRect;
}

// EVERY registered region in this application now carries a tagged context — the morph dials were
// the last holdout, and they carried a bare integer. That is what makes this variant safe: it walks
// all layers, so the fixed morph overlay is found before the scrolling canvas underneath it, exactly
// as a click resolves.
const tCanvasWidget * canvas_widget_at_any_layer(tCoord coord) {
    return (const tCanvasWidget *)click_region_at(coord);
}

const tCanvasWidget * canvas_widget_at(tCoord coord) {
    // Canvas layer only — see moduleGraphics.h. The morph dials sit at eClickLayerPanel and register
    // an INTEGER cast to a pointer as their user data, so reading a tag off one would not merely be
    // the wrong answer, it would dereference a small integer.
    return (const tCanvasWidget *)click_region_at_layer(coord, eClickLayerCanvas);
}

uint32_t canvas_widget_index(const tCanvasWidget * widget) {
    if (widget == NULL) {
        return 0;
    }

    switch (widget->kind) {
        case eCanvasWidgetParam:
            return ((const tParamClickCtx *)widget)->paramIndex;

        case eCanvasWidgetMode:
            return ((const tModeClickCtx *)widget)->modeIndex;

        case eCanvasWidgetConnector:
            return ((const tConnectorClickCtx *)widget)->connectorIndex;

        default:
            return 0;
    }
}

// Is this parameter the thing under the cursor RIGHT NOW?
//
// Answered from the click-region registry rather than by testing the app's own rectangle array, so
// the answer cannot disagree with where a click would actually land: same front-to-back walk, same
// data, one description of where the widget is. The comparison is against this parameter's own click
// context, which is its identity in the registry — no tag, no lookup table, and sParamClickCtx stays
// private to this file.
//
// Returns false while the registry is empty (before the first frame) or when something is drawn over
// the canvas, which is the correct answer in both cases.
bool param_is_under_cursor(const tModule * module, uint32_t paramIndex, tCoord coord) {
    if ((module == NULL) || (paramIndex >= MAX_NUM_PARAMETERS)) {
        return false;
    }
    return click_region_at(coord) == &sParamClickCtx[module->key.slot][module->key.location][module->key.index][paramIndex];
}

void render_mode_common(tRectangle rectangle, tModule * module, uint32_t modeRef, uint32_t modeIndex) {
    // The caller's loop is bounded by module_mode_count(), which counts ROWS IN modeLocationList for
    // this type — not by MAX_NUM_MODES, which is how many a tModule can hold. Those were the same
    // thing while MAX_NUM_MODES was 16; at 2 they are one added table row apart, and the writes below
    // would run off the end of module->mode[] and off sModeClickCtx's last dimension.
    if (modeIndex >= MAX_NUM_MODES) {
        LOG_ERROR("MAX_NUM_MODES needs increasing to >= %u (module type %u)\n", modeIndex + 1, module->type);
        EXIT_IN_DEBUG();
        return;
    }
    uint32_t modeValue = module->mode[modeIndex].value;

    // Per MODE, not per module: mode[0] took every mode's modeRef, so the LAST one rendered won and
    // every other mode kept 0 — modeLocationList[0], which is OscShpB's waveform. That is why the
    // Gate's second drop-down opened a menu of Sine1/Sine2/..., and why picking from it could write a
    // value the Gate has no meaning for (that list is 8 long, gateTypeStrMap is 6).
    module->mode[modeIndex].modeRef = modeRef;

    switch (modeLocationList[modeRef].type) {
        case paramTypeOscWave:
        {
            char       buff[16]     = {0};

            snprintf(buff, sizeof(buff), "%u", modeValue);
            // render_dial_with_text() is dial-anchored and draws its text upwards, so shift the
            // rect down by the rows this mode will use to keep the block where it was. No entry
            // in modeLocationList is currently an OscWave, so this path is unexercised - it is
            // converted for correctness rather than because anything renders through it today.
            double     modeLabelH   = rectangle.size.h / 4.0;
            tRectangle modeDialRect = rectangle;

            modeDialRect.coord.y                                                               += (modeLocationList[modeRef].label != NULL) ? (modeLabelH * 2.0) : modeLabelH;
            modeDialRect.size.h                                                                 = modeDialRect.size.w;
            module->mode[modeIndex].rectangle                                                   = render_dial_with_text(moduleArea, modeDialRect, (char *)modeLocationList[modeRef].label, buff, modeLabelH, modeValue, modeLocationList[modeRef].range, 0, (tRgb)RGB_GREY_5); // TODO: Check if Mode can be morphed
            sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex] = (tModeClickCtx){
                eCanvasWidgetMode, module->key, modeIndex
            };
            register_click_region(module->mode[modeIndex].rectangle, eClickLayerCanvas, mode_click_handler,
                                  &sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex]);
            break;
        }
        case paramTypeToggle:
        case paramTypeMenu:
        {
            const char ** strMap     = modeLocationList[modeRef].strMap;
            double        y          = rectangle.coord.y;
            double        textHeight = rectangle.size.h / 2.0;

            //LOG_DEBUG("Mode for module %s\n", gModuleProperties[module->type].name);
            if (strMap == NULL) {
                LOG_ERROR("No strMap for module type %s\n", gModuleProperties[module->type].name);

                //Debug help for value
                char debug[64] = {0};
                snprintf(debug, sizeof(debug), "modeRef %u", modeRef);
                draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {30, textHeight}}, debug, (tRgb)RGB_BACKGROUND_GREY);
                return;
            }
            //if (paramLocationList[paramRef].colourMap != NULL) {
            //    set_rgb_colour(paramLocationList[paramRef].colourMap[paramValue]);
            //} else {
            //    set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);
            //}

            // The label sits ABOVE the button, and the button does not move to make room. The dial
            // branch above does the opposite — it offsets the dial downwards — but every position in
            // modeLocationList was laid out against a renderer that drew no label at all, so pushing
            // the buttons down would shift every mode dropdown in the app. Drawing upwards into the
            // space the module already leaves keeps those positions meaning what they always did.
            if (modeLocationList[modeRef].label != NULL) {
                set_rgb_colour((tRgb)RGB_BLACK);
                render_text(moduleArea,
                            (tRectangle){{rectangle.coord.x, y - textHeight}, {rectangle.size.w, textHeight}},
                            (char *)modeLocationList[modeRef].label);
            }
            // A waveform picker shows the WAVE, not its name — see module_wave_picker_mode(). The
            // caption is blanked so the picture has the button to itself, but the width still comes
            // from text, as everywhere else: WAVE_MENU_CAPTION reserves a face wide enough to read a
            // waveform in, where an empty string would collapse the button to a sliver.
            bool isWaves = module_wave_picker_mode(module->type, modeIndex);

            module->mode[modeIndex].rectangle                                                   = draw_button(moduleArea, (tRectangle){{rectangle.coord.x, y}, {isWaves ? get_text_width(WAVE_MENU_CAPTION, textHeight, eNoCache) : largest_text_width(modeLocationList[modeRef].range, strMap, textHeight, eCache), textHeight}}, isWaves ? "" : strMap[modeValue], (tRgb)RGB_BACKGROUND_GREY);

            if (isWaves == true) {
                render_wave_icon(module->mode[modeIndex].rectangle, module->type, modeValue,
                                 module_wave_icon_shape(module->type));
            }
            sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex] = (tModeClickCtx){
                eCanvasWidgetMode, module->key, modeIndex
            };
            register_click_region(module->mode[modeIndex].rectangle, eClickLayerCanvas, mode_click_handler,
                                  &sModeClickCtx[module->key.slot][module->key.location][module->key.index][modeIndex]);
            break;
        }
        default:
        {
        }
        break;
    }
}

void render_volume_common(tRectangle rectangle, tModule * module, uint32_t volumeRef, uint32_t volumeIndex) {
    module->volume.volumeRef = volumeRef;

    switch (volumeLocationList[volumeRef].volumeType) {
        case volumeTypeMono:
        {
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]);
        }
        break;
        case volumeTypeStereo:
        {
            double space = find_volume_meter_config(volumeTypeStereo)->space;

            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[1]);
        }
        break;
        case volumeTypeSequencer:
        {
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
        }
        break;
        case volumeTypeQuad:
        {
            double space = find_volume_meter_config(volumeTypeQuad)->space;

            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]); // TODO: Should come from volume location list!? Shouldn't be in gModuleProperties
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[1]);
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[2]);
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[3]);
        }
        break;
        case volumeTypeCompress:
        {
            render_volume_meter(rectangle, volumeLocationList[volumeRef].volumeType, module->volume.value[0]);
        }
        break;
        default:
        {
        }
        break;
    }
}

// A read-only readout — see tDisplayLocation. No click region is registered for it: it is not a
// control, and giving it one would put a dead target over the module body where a right-click should
// still reach the module's own menu.
void render_display_common(tRectangle rectangle, tModule * module, uint32_t displayRef) {
    uint32_t     slot       = module->key.slot;
    uint32_t     variation  = gPatchDescr[slot].activeVariation;
    uint32_t     source     = displayLocationList[displayRef].sourceParam;
    const char * label      = displayLocationList[displayRef].label;
    char         buff[16]   = {0};
    double       textHeight = (double)STANDARD_TEXT_HEIGHT;

    if (source >= MAX_NUM_PARAMETERS) {
        return;
    }

    switch (displayLocationList[displayRef].displayType) {
        case displayTypeSwitchCtrl:
        {
            // Four per step, which is the whole point of the number: it is what the Mux modules
            // decode back into a channel. Calculated, not tabulated — the rule is the same for a
            // two-way switch as for an eight-way one, which is exactly why the G2 uses it.
            snprintf(buff, sizeof(buff), "%u", module->param[variation][source].value * SWITCH_CTRL_STEP);
            break;
        }

        default:
            return;
    }
    // PLAIN TEXT ON THE FACE, not a button. It is a readout — nothing here can be clicked — and
    // drawing it as a button invited the eye to try. No click region is registered either, so a
    // right-click over it still reaches the module's own menu.
    set_rgb_colour((tRgb)RGB_BLACK);
    render_text(moduleArea, (tRectangle){{rectangle.coord.x, rectangle.coord.y}, {BLANK_SIZE, textHeight}}, buff);

    if (label == NULL) {
        return;
    }
    tRectangle labelRect = {{rectangle.coord.x, rectangle.coord.y}, {BLANK_SIZE, textHeight}};

    // Placed the same four ways a connector's label is, and for the same reasons — see
    // render_connector_common(), which this deliberately mirrors rather than inventing its own rule.
    switch (displayLocationList[displayRef].labelLoc) {
        case labelLocUp:
            labelRect.coord.y -= textHeight;
            break;

        case labelLocDown:
            labelRect.coord.y += (rectangle.size.h + 2.0);
            break;

        case labelLocLeft:
            labelRect.coord.x -= (get_text_width((char *)label, textHeight, eCache) + 2.0);
            break;

        case labelLocRight:
            labelRect.coord.x += (rectangle.size.w + 2.0);
            break;
    }
    render_text(moduleArea, labelRect, (char *)label);
}

void render_led_common(tRectangle rectangle, tModule * module, uint32_t ledRef, uint32_t ledIndex) {
    switch (ledLocationList[ledRef].ledType) {
        case ledTypeMultiBit:  // one bit of a group value rather than a 2-bit value of its own
        case ledTypeYes:
        {
            // Same bound the parser applies, from the other end: the caller's loop counts rows in
            // ledLocationList, and nothing stops that table growing a ninth row for a type.
            if (ledIndex >= MAX_LEDS_PER_MODULE) {
                LOG_ERROR("MAX_LEDS_PER_MODULE needs increasing to >= %u (module type %u)\n", ledIndex + 1, module->type);
                EXIT_IN_DEBUG();
                break;
            }
            // Bit 0 green, bit 1 red — the pair to parse_led_data()'s extraction, and swapped from
            // what this used to say. The two were mirror images of each other and cancelled exactly
            // (a value of 1 or 2 was transposed on the way in and transposed back here; 0 and 3 are
            // symmetric), so this draws the same colours it always did. Which of the two bits is
            // really green is still an assumption — but it is now ONE assumption, in one place,
            // instead of two that only worked together.
            uint32_t ledVal = module->led.value[ledIndex];
            bool     green  = ledVal & 1;
            bool     red    = (ledVal >> 1) & 1;

            if (green && red) {
                set_rgb_colour((tRgb)RGB_YELLOW_7);
            } else if (green) {
                set_rgb_colour((tRgb)RGB_GREEN_7);
            } else if (red) {
                set_rgb_colour((tRgb)RGB_RED_7);
            } else {
                set_rgb_colour((tRgb)RGB_BLACK);
            }
            render_rectangle(moduleArea, rectangle);
            break;
        }
        case ledTypePark:
        {
            set_rgb_colour((tRgb)RGB_GREEN_3);
            render_rectangle(moduleArea, rectangle);
            break;
        }
        default:
        {
        }
        break;
    }
}

void render_connector_common(tRectangle rectangle, tModule * module, tConnectorDir dir, tConnectorType type, uint32_t connectorListIndex, uint32_t connectorIndex) {
    if (connectorIndex >= MAX_NUM_CONNECTORS) {
        LOG_ERROR("MAX_NUM_CONNECTORS needs increasing to >= %u\n", connectorIndex + 1);
        exit(1);
    }
    module->connector[connectorIndex].coord = rectangle.coord;  // Register where we're rendering this connector, for cable connecting
    // .dir and .type are deliberately NOT written here any more. They are static per module type
    // and are now filled by populate_module_connectors() when the module enters the database, so
    // they are correct for code that never draws — the sound engine's cable lookups above all.
    // Setting them here as well would restore two owners for one fact, which is how they came to be
    // available only after rendering in the first place. Geometry stays ours; the rest is the
    // resource list's.

    if (connectorLocationList[connectorListIndex].label != NULL) {
        tRectangle textRectangle = rectangle;
        textRectangle.size.w = BLANK_SIZE;
        textRectangle.size.h = STANDARD_TEXT_HEIGHT;

        set_rgb_colour((tRgb)RGB_BLACK);

        switch (connectorLocationList[connectorListIndex].labelLoc) {
            case labelLocUp:
                textRectangle.coord.y -= STANDARD_TEXT_HEIGHT; // May need scaling
                break;
            case labelLocDown:
                // PAST THE CONNECTOR, not by a text height. This used to add STANDARD_TEXT_HEIGHT, which
                // is a measurement of the LABEL and says nothing about how tall the thing it has to clear
                // is — so the glyphs landed on the bottom of the connector circle. labelLocLeft and
                // labelLocRight already step over the connector by its own size.w; this is the same rule
                // on the other axis, and it stays correct whatever CONNECTOR_SIZE or the zoom become.
                //
                // labelLocUp is left as it is: going UP, the distance to clear is the label's own height,
                // because coord.y is the top of the text box.
                textRectangle.coord.y += (rectangle.size.h + 2);
                break;
            case labelLocLeft:
                textRectangle.coord.x -= (get_text_width((char *)connectorLocationList[connectorListIndex].label, textRectangle.size.h, eCache) + 2);
                textRectangle.coord.y += 2;
                break;
            case labelLocRight:
                textRectangle.coord.x += (rectangle.size.w + 2);
                textRectangle.coord.y += 2;
                break;
        }
        render_text(moduleArea, textRectangle, (char *)connectorLocationList[connectorListIndex].label);
    }
    // Per the G2 manual ("Control signals, blue connectors" / "Logic or gate signals, yellow and
    // orange connectors", g2manual.txt p.135) and confirmed against the original editor's behaviour
    // (Original Editor/G2Editor.c — CPnlControlInHole/OutHole::GetColor() and
    // CPnlLogicInHole/OutHole::GetColor(), both bandwidth-dependent; ECableColor's own
    // TurboLogic entry, RGB (1.0, 0.75, 0.31), confirms the exact orange): a module running at the
    // higher (audio) bandwidth promotes its blue (control) connectors to red/Audio, but a yellow
    // (logic) connector instead becomes orange/TurboLogic — still a logic signal, just the
    // higher-bandwidth variant, never plain red. NOT stored back into
    // module->connector[connectorIndex].type above — that field is the connector's permanent
    // declared type, used by protocol.c's own upRate-propagation walk (see its own comment), and
    // must never reflect this purely-cosmetic promotion.
    set_rgb_colour(connectorColourMap[effective_connector_type(type, module->upRate)]);  // Note, was using "module->connector[connectorIndex].type", check that this type param is OK

    // The passed-in dir, not module->connector[].dir — they hold the same value from the same
    // connectorLocationList entry, and using the parameter keeps this drawing code independent of
    // when the array happens to have been filled.
    if (dir == connectorDirIn) {
        module->connector[connectorIndex].rectangle = render_circle_part(moduleArea, (tCoord){rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)}, rectangle.size.w / 2.0, 10.0, 0.0, 10.0);
    } else {
        module->connector[connectorIndex].rectangle = render_rectangle(moduleArea, (tRectangle){rectangle.coord, {rectangle.size.w, rectangle.size.h}});
    }
    // The stored rectangle is the HIT area, not the drawn one — see CONNECTOR_HIT_PADDING. It is the
    // only thing .rectangle is used for (this registration, handle_cable_connect()'s release target
    // and the hover test), so padding it here makes all three agree; the cable end itself is drawn
    // from .coord, which is untouched.
    module->connector[connectorIndex].rectangle.coord.x                                          -= CONNECTOR_HIT_PADDING;
    module->connector[connectorIndex].rectangle.coord.y                                          -= CONNECTOR_HIT_PADDING;
    module->connector[connectorIndex].rectangle.size.w                                           += 2.0 * CONNECTOR_HIT_PADDING;
    module->connector[connectorIndex].rectangle.size.h                                           += 2.0 * CONNECTOR_HIT_PADDING;

    sConnectorClickCtx[module->key.slot][module->key.location][module->key.index][connectorIndex] = (tConnectorClickCtx){
        eCanvasWidgetConnector, module->key, connectorIndex
    };
    register_click_region(module->connector[connectorIndex].rectangle, eClickLayerCanvas, connector_click_handler,
                          &sConnectorClickCtx[module->key.slot][module->key.location][module->key.index][connectorIndex]);
    set_rgb_colour((tRgb)RGB_BLACK);
    render_circle_part(moduleArea, (tCoord){rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)}, rectangle.size.w / 4.0, 10.0, 0.0, 10.0);
}

tRectangle adjust_rectangle(tRectangle moduleBase, tRectangle relative, tAnchor anchor, tModule * module) {
    relative = rectangle_scale_from_percent(relative);

    switch (anchor) {
        case anchorTopLeft:
            relative.coord.x = moduleBase.coord.x + relative.coord.x;
            relative.coord.y = moduleBase.coord.y + relative.coord.y;
            break;
        case anchorTopRight:
            relative.coord.x = ((moduleBase.coord.x + moduleBase.size.w) + relative.coord.x) - relative.size.w;
            relative.coord.y = moduleBase.coord.y + relative.coord.y;
            break;
        case anchorTopMiddle:
            relative.coord.x = ((moduleBase.coord.x + (moduleBase.size.w / 2.0)) + relative.coord.x) - (relative.size.w / 2.0);
            relative.coord.y = moduleBase.coord.y + relative.coord.y;
            break;
        case anchorMiddleLeft:
            relative.coord.x = moduleBase.coord.x + relative.coord.x;
            relative.coord.y = ((moduleBase.coord.y + (moduleBase.size.h / 2.0)) + relative.coord.y) - (relative.size.h / 2.0);
            break;
        case anchorMiddleRight:
            relative.coord.x = ((moduleBase.coord.x + moduleBase.size.w) + relative.coord.x) - relative.size.w;
            relative.coord.y = ((moduleBase.coord.y + (moduleBase.size.h / 2.0)) + relative.coord.y) - (relative.size.h / 2.0);
            break;
        case anchorMiddle:
            relative.coord.x = ((moduleBase.coord.x + (moduleBase.size.w / 2.0)) + relative.coord.x) - (relative.size.w / 2.0);
            relative.coord.y = ((moduleBase.coord.y + (moduleBase.size.h / 2.0)) + relative.coord.y) - (relative.size.h / 2.0);
            break;
        case anchorBottomLeft:
            relative.coord.x = moduleBase.coord.x + relative.coord.x;
            relative.coord.y = ((moduleBase.coord.y + moduleBase.size.h) + relative.coord.y) - relative.size.h;
            break;
        case anchorBottomMiddle:
            relative.coord.x = ((moduleBase.coord.x + (moduleBase.size.w / 2.0)) + relative.coord.x) - (relative.size.w / 2.0);
            relative.coord.y = ((moduleBase.coord.y + moduleBase.size.h) + relative.coord.y) - relative.size.h;
            break;
        case anchorBottomRight:
            relative.coord.x = ((moduleBase.coord.x + moduleBase.size.w) + relative.coord.x) - relative.size.w;
            relative.coord.y = ((moduleBase.coord.y + moduleBase.size.h) + relative.coord.y) - relative.size.h;
            break;
    }
    return relative;
}

// Registers module->connector[i].coord (logical, moduleArea-local — the same space cables
// read it back in, applying scale/scroll themselves at draw time) and draws the small
// connector glyphs. Split out of render_module_common() so it can run on its own for a
// module that's currently scrolled off-screen: cables reference connector positions on
// BOTH their endpoint modules regardless of which one (if either) is actually visible right
// now, so this must stay up to date even when the rest of that module's rendering is skipped.
static void render_module_connectors(tRectangle rectangle, tModule * module) {
    uint32_t connector = 0;

    for (uint32_t i = module->connectorIndexCache; i < array_size_connector_location_list(); i++) {
        if (connectorLocationList[i].moduleType == module->type) {
            if (module->gotConnectorIndexCache == false) {
                module->connectorIndexCache    = i;
                module->gotConnectorIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, connectorLocationList[i].rectangle, connectorLocationList[i].anchor, module);
            adjusted.size.h = adjusted.size.w; // We want this one to be square
            render_connector_common(adjusted, module, connectorLocationList[i].direction, connectorLocationList[i].type, i, connector++);

            if (connector >= module_connector_count(module->type)) {
                break;
            }
        }
    }
}

// ── OscShpB waveform preview ─────────────────────────────────────────────────
//
// The manual describes "Waveform Drop-Down Selectors With Graphs" on the G2's
// Shape Oscillators. This was originally built against OscB, using per-
// waveform routines recovered by inspection
// (CPnlWaveformGraphABC::DrawSine/DrawTri/DrawSaw/DrawSquare/DrawDsf) - but
// on real hardware, Shape turned out to make no audible difference to OscB's
// sin/tri/saw at all (only squ and sup). CPnlOscSinShapeGraph, the one
// reference graph-widget class with "Shape" in its name, also never quite
// matched OscB's 5 waveforms - its Draw() dispatches to DrawDualSine/
// DrawDsf/DrawTweekTri/DrawPulse, which fits OscShpB's 8-option waveform
// mode (Sine1-4, TriSaw, DblSaw, Pulse, SymPulse - oscShpBStrMap) far
// better. Moved here on that basis; still an approximation distilled into
// simple closed-form functions rather than a byte-exact port, so treat this
// as a starting point to verify against real hardware, same as before.
// A single cycle that starts at a rising zero-crossing and runs -1..+1..-1..(back to 0), with
// "peak" (0..1) setting where the top of the ramp falls: 0.5 is a symmetric triangle, near 1.0 a
// near-full sawtooth ramp. This is exactly TriSaw's math below, factored out so DblSaw can reuse
// it rather than duplicate a subtly different version of the same thing - both the phase-shift
// (landing a rising, not falling, zero-crossing at phase 0) and the sign flip (matching the real
// editor's orientation) were only worked out and confirmed via TriSaw.
static double skewed_ramp_zero_start(double phase, double peak) {
    double p = fmod(((peak + 1.0) * 0.5) + phase, 1.0);
    double y = (p < peak) ? (((p / peak) * 2.0) - 1.0) : ((((1.0 - p) / (1.0 - peak)) * 2.0) - 1.0);

    return -y;
}

// Ramp width for the Pulse/SymPulse zero-crossing ramps below: half a sample-to-sample step at
// the render loop's 200-sample resolution (must stay narrower than that step or the ramp isn't
// actually sampled at all), capped further so it never eats more than half of whatever room is
// actually available on either side of it (relevant once High/Low get thin near Shape's
// extremes - see SymPulse below).
static double pulse_edge_width(double roomAvailable) {
    return fmin(0.0025, roomAvailable * 0.5);
}

// The two ends of a Pulse/SymPulse ramp: a hard step never actually produces a sample AT zero,
// so both add these explicit narrow ramps at their zero-crossing points instead of relying on
// the step happening to land on a sample.
static double ramp_from_zero(double phaseIntoRamp, double edgeWidth) {
    return phaseIntoRamp / edgeWidth; // 0 -> +1
}

static double ramp_to_zero(double phaseIntoRamp, double edgeWidth) {
    return -1.0 + (phaseIntoRamp / edgeWidth); // -1 -> 0
}

// Shape is always the raw 0-127 param value normalised to 0..1 - but the dial itself only
// *displays* 50%..99% of that (render_paramType1Shape), so Shape 0 is the dial's displayed
// minimum (50%) and Shape 1 its displayed maximum (99%), not "no shaping"/"full shaping" in the
// usual 0-100% sense. Sine1-4/TriSaw/Pulse/SymPulse are all taken directly from the manual's
// "WAVEFORMS AND SHAPES" section for OscShpB (g2manual.txt), which spells out each one's exact
// Shape 50%/75%/99% appearance - not guesses. DblSaw's manual description hasn't been
// reconciled with what's actually implemented below yet (see its own comment).
static double oscshpb_waveform_sample(uint32_t waveformIndex, double phase, double shape) {
    // THE LAWS LIVE IN waveModels.c, shared with the sound engine so the wave that is drawn and the
    // wave that is heard cannot drift apart — which they had, badly, before it existed. What stays
    // here is how a law is turned into a DRAWN curve, and that is genuinely this file's business:
    // the render loop takes 200 samples, so a hard step between two of them is simply missed and the
    // drawn line never passes through zero where the real wave does. Hence the explicit ramps below,
    // which the engine neither has nor wants — it band-limits instead.
    switch (waveformIndex) {
        case 0:
        case 1:
        case 2:
        case 3:
            // Closed-form and continuous: nothing to decide, so the shared value is used as it is.
            return wave_sine_by_index(waveformIndex, phase, shape);

        case 4: // TriSaw
        {
            return skewed_ramp_zero_start(phase, wave_trisaw_peak(shape));
        }
        case 5: // DblSaw — two near-full sawtooths, the second detuned
        {
            double detune = wave_dblsaw_detune(shape);

            return -(skewed_ramp_zero_start(phase, WAVE_DBLSAW_PEAK)
                     + skewed_ramp_zero_start(phase + detune, WAVE_DBLSAW_PEAK)) * 0.5;
        }
        case 6: // Pulse — asymmetric width
        {
            double duty      = wave_pulse_duty(shape);
            double edgeWidth = pulse_edge_width(fmin(duty, 1.0 - duty));

            if (phase < edgeWidth) {
                return ramp_from_zero(phase, edgeWidth);
            }

            if (phase >= (1.0 - edgeWidth)) {
                return ramp_to_zero(phase - (1.0 - edgeWidth), edgeWidth);
            }
            return (phase < duty) ? 1.0 : -1.0;
        }
        case 7: // SymPulse — High, then Low, then silence for the remainder of the cycle
        {
            double halfSeg   = wave_sympulse_half_segment(shape);
            double edgeWidth = pulse_edge_width(halfSeg);

            if (phase < edgeWidth) {
                return ramp_from_zero(phase, edgeWidth);
            }

            if (phase < halfSeg) {
                return 1.0;
            }

            if (phase < (2.0 * halfSeg)) {
                return -1.0;
            }

            if (phase < ((2.0 * halfSeg) + edgeWidth)) {
                return ramp_to_zero(phase - (2.0 * halfSeg), edgeWidth);
            }
            return 0.0;
        }
        default:
            return 0.0;
    }
}

// LfoShpA's six waves are a DIFFERENT FAMILY from the two shape oscillators' - Sine, CosBell,
// TriBell, Saw2Tri, Sqr2Tri, Sqr - so none of OscShpB's laws carry over wholesale. MEASURED
// 2026-08-23 by running the LFO at audio rate (Range = Rate Hi) so the same capture rig applies,
// then fitting candidate families against the captured cycle; every wave landed on one cleanly,
// at 0.98 to 1.000.
//
// ITS SHAPE DIAL IS BIPOLAR AND DEFAULTS TO 64, unlike the oscillators' Shape which starts at 0 and
// only opens. At 64 each wave is its neutral self - a pure sine, a symmetric triangle, a square -
// and Shape skews it either way from there. paramLocationList already carries that default.
static double lfoshpa_waveform_sample(uint32_t waveformIndex, double phase, double shape) {
    switch (waveformIndex) {
        case 0:
        {
            // Sine - THE SAME three-segment symmetric phase warp as OscShpB's Sine1, which is a
            // pleasing result: one model now serves OscShpA, OscShpB and this. Only the breakpoint
            // law differs, and here it is linear and passes through the identity (0.25) at the
            // dial's centre: measured 0.02, 0.13, 0.25, 0.37, 0.48 across the sweep.
            double b = 0.02 + (0.46 * shape);
            double w = 0.0;

            if (b < 0.02) {
                b = 0.02;
            }

            if (phase < b) {
                w = 0.25 * (phase / b);
            } else if (phase < (1.0 - b)) {
                w = 0.25 + (0.5 * ((phase - b) / (1.0 - (2.0 * b))));
            } else {
                w = 0.75 + (0.25 * ((phase - (1.0 - b)) / b));
            }
            return sin(2.0 * M_PI * w);
        }
        case 1:
        case 2:
        {
            // CosBell and TriBell - one bell per cycle, silent for the rest of it, and the bell simply
            // WIDENS with Shape: measured width 0.05, 0.26, 0.50, 0.74, 0.98, i.e. the dial itself.
            // The two differ only in the bell's own profile, a raised cosine against a triangle.
            // The output is AC coupled, so the bell's mean is removed - exactly 0.5 * width for both
            // profiles - and what is left is renormalised, which is what puts the bell above the line
            // and a shallow negative shelf below it rather than a bell sitting on zero.
            // Neither bell closes completely, and they do not stop at the same place: at Shape 0
            // CosBell measures about 0.08 wide and TriBell about 0.05. Forcing both to one figure
            // costs the other one visibly, so the floor is per wave.
            double floorWidth = (waveformIndex == 1) ? 0.08 : 0.05;
            double width      = shape;
            double y          = 0.0;

            if (width < floorWidth) {
                width = floorWidth;
            }

            if (phase < width) {
                double u = phase / width;

                y = (waveformIndex == 1) ? (0.5 * (1.0 - cos(2.0 * M_PI * u)))
                    : ((u < 0.5) ? (u * 2.0) : ((1.0 - u) * 2.0));
            }
            double mean       = 0.5 * width;
            double peak       = fmax(1.0 - mean, mean);

            return (y - mean) / peak;
        }
        case 3:
        {
            // Saw2Tri - a triangle whose APEX slides right across the cycle, so it is a falling saw at
            // one end of the dial, a symmetric triangle at the centre and a rising saw at the other.
            // Measured apex 0.02, 0.26, 0.50, 0.74, 0.98 - the dial again, near enough exactly.
            double apex = 0.02 + (0.96 * shape);

            return (phase < apex) ? ((phase / apex) * 2.0 - 1.0)
                    : (((1.0 - phase) / (1.0 - apex)) * 2.0 - 1.0);
        }
        case 4:
        {
            // Sqr2Tri - a symmetric triangle driven progressively harder into a clip, so it fills out
            // from triangle through trapezoid towards a square. Measured gain 1.0, 2.0, 3.0, 3.9, 4.9,
            // which is 1 + 3.9 * Shape and reaches a trapezoid rather than a true square at the top.
            double gain = 1.0 + (3.9 * shape);
            double tri  = (phase < 0.5) ? ((phase / 0.5) * 2.0 - 1.0) : (((1.0 - phase) / 0.5) * 2.0 - 1.0);
            double y    = gain * tri;

            return (y > 1.0) ? 1.0 : ((y < -1.0) ? -1.0 : y);
        }
        default:
        {
            // Sqr - a plain pulse width, and the width IS the dial: measured 0.26, 0.50, 0.74, 0.96.
            // It correlates a little lower than the others (0.90 to 0.99) because the instrument's
            // edges are not vertical, which a drawn icon has no way to show at this size anyway.
            // Measured 0.08, 0.26, 0.50, 0.74, 0.96 - very nearly the dial itself, but it neither
            // closes nor fills completely, so the fitted line and its floor are used rather than the
            // bare dial value.
            double duty = 0.024 + (0.936 * shape);

            if (duty < 0.08) {
                duty = 0.08;
            }
            return (phase < duty) ? 1.0 : -1.0;
        }
    }
}

// LfoB has NO Shape parameter — it picks one of four fixed waveforms, so its graph is one static
// shape per selection rather than a morphing one. Captured 2026-08-23 by running the LFO at audio
// rate (Range = Rate Hi), the same trick that makes any of these measurable with the audio rig.
//
// ITS WAVE PARAMETER STOPS AT 3, even though lfoWaveStrMap carries six names: setting 4 or 5 gives a
// cycle indistinguishable from Squ, so the device clamps there. paramLocationList's declared range of
// 4 is right, and the map's trailing "RndSt"/"Rnd" belong to a different LFO. Random waves could not
// be drawn as a single cycle in any case.
// THE FOUR TEXTBOOK WAVES, as four primitives rather than four copies. LfoB's measured cycle and the
// OscA family's first four are the same shapes, and every one of the G2's plain (non-shapable)
// waveform selectors is built out of these — so they are written once here and the per-module sample
// functions below choose among them. The bodies came from LfoB's capture and are unchanged by being
// named: this is the same code it always ran.
static double basic_sine(double phase) {
    return sin(2.0 * M_PI * phase);
}

// Symmetric triangle, apex at the quarter cycle so it starts at a rising zero.
static double basic_triangle(double phase) {
    return (phase < 0.25) ? (phase * 4.0)
           : ((phase < 0.75) ? (2.0 - (phase * 4.0)) : ((phase * 4.0) - 4.0));
}

// FALLS across the cycle, the same direction as TriSaw's ramp — and the opposite of DblSaw's and of
// OscA's, both of which the captures show rise. THREE MEASUREMENTS, TWO DIRECTIONS: saw polarity is
// per-module on this instrument and must never be carried from one module to another unmeasured.
static double basic_falling_saw(double phase) {
    return 1.0 - (2.0 * phase);
}

// RISES across the cycle. OscA's saw, measured 2026-08-24: correlating the captured cycle against the
// falling form scored 0.516 and against the rising one 0.988 — the same test, and the same size of
// gap, that caught DblSaw drawn upside down.
static double basic_rising_saw(double phase) {
    return (2.0 * phase) - 1.0;
}

// High for the first `duty` of the cycle, low for the rest. Full swing, with no DC removal: that is
// what LfoB's Squ measured as, and what OscShpB's measured Pulse draws at every width.
static double basic_pulse(double phase, double duty) {
    return (phase < duty) ? 1.0 : -1.0;
}

static double lfob_waveform_sample(uint32_t waveformIndex, double phase) {
    switch (waveformIndex) {
        case 0:
            return basic_sine(phase);              // Sin

        case 1:
            return basic_triangle(phase);          // Tri

        case 2:
            return basic_falling_saw(phase);       // Saw

        default:
            return basic_pulse(phase, 0.5);        // Squ
    }
}

// RECT AND SHPSTATIC ARE NOT WAVES AT ALL — they are TRANSFER CURVES, so their icon plots output
// against INPUT rather than against phase, and the horizontal axis runs -1 to +1 instead of round a
// cycle. Everything else about the picker is the same, which is why they arrive through the same
// plumbing: module_wave_sample() is handed a position across the box either way.
//
// MEASURED 2026-08-24 with a dry/wet rig — a triangle sent BOTH straight to one output and through
// the shaper to the other, so pairing the two channels sample by sample gives the transfer curve
// directly, with the source cancelling out. The fit leaves the gain free, because the two channels
// have unknown relative gain and an input that never reaches full scale would otherwise masquerade
// as curvature (it did: peak-normalising first pulled every exponent toward 1).
//
// RECT IS EXACTLY WHAT THE MANUAL SAYS (p207), which is worth recording given how often it is not:
// discard negatives, discard positives, mirror negatives up, mirror positives down.
//
// SHPSTATIC IS y = sign(x).|x|^p, and the two positive powers are exact:
//     x2      p = 1.98   rms 0.00002      x3      p = 2.97   rms 0.00002
//     Inv x2  p = 0.65   rms 0.00079      Inv x3  p = 0.49   rms 0.00122
// The two inverse curves fit a pure power law FORTY TIMES WORSE than the other two and land well
// above their nominal 1/2 and 1/3, so those exponents are the measured shape rather than the named
// one, and the shape is only approximately a power law. Good enough for a 30-pixel icon; worth a
// second look before anything depends on it more precisely than that.
static double shaper_transfer_sample(uint32_t moduleType, uint32_t modeValue, double input) {
    if (moduleType == moduleTypeRect) {
        switch (modeValue) {
            case 0:
                return (input > 0.0) ? input : 0.0;         // HalfPos — discards negatives

            case 1:
                return (input < 0.0) ? input : 0.0;         // HalfNeg — discards positives

            case 2:
                return fabs(input);                         // FullPos — mirrors negatives up

            default:
                return -fabs(input);                        // FullNeg — mirrors positives down
        }
    }
    {
        // shpStaticStrMap order is Inv x3, Inv x2, x2, x3 — gentlest inverse first.
        static const double exponents[] = {0.49, 0.65, 1.98, 2.97};
        uint32_t            index       = (modeValue < 4) ? modeValue : 0;
        double              magnitude   = pow(fabs(input), exponents[index]);

        return (input < 0.0) ? -magnitude : magnitude;
    }
}

// A transfer curve has ENDS, not a seam. render_wave_icon() closes a cycle by drawing the step
// between its last sample and its first, which is right for a saw and nonsense here — the two ends of
// a rectifier's curve are simply its extremes, and joining them would draw a vertical through the
// middle of the picture.
static bool module_wave_is_transfer(uint32_t moduleType) {
    return (moduleType == moduleTypeRect) || (moduleType == moduleTypeShpStatic);
}

// LfoA's and LfoC's set — lfoWaveStrMap, the SAME map LfoB uses, but all six entries rather than the
// four LfoB clamps to. The first four are LfoB's measured cycle and are simply delegated; only the
// two random ones are new, and they are a different kind of thing entirely.
//
// A RANDOM WAVE HAS NO CYCLE TO MEASURE, so this is the one wave icon in the app that is a
// REPRESENTATION rather than a fitted law, and it should not pretend otherwise. What it has to convey
// is the single distinction between the two entries: RndSt holds each new value until the next step
// (a sample-and-hold staircase) while Rnd glides between them. Everything else about it — how many
// steps, which levels — is a drawing decision.
//
// THE LEVELS ARE A FIXED TABLE, deliberately. Drawing from an actual random source would make the
// icon flicker on every redraw, which is worse than useless on a picker: two entries that never look
// the same twice cannot be compared. Six steps reads clearly at the ~30 pixels a button gives.
//
// MEASURED 2026-08-24, and the direction is confirmed. LfoA at audio rate (Range = Rate Hi, the trick
// that made LfoShpA measurable), each wave captured to ITS OWN file so there are no sweep boundaries
// to mis-segment. Read from the distribution of sample-to-sample differences, since a staircase is
// bimodal — nearly all differences zero, a few very large — where a glide is not:
//                p50/p99      differences at zero    longest hold
//     RndSt        0.057            43.4%             19 samples
//     Rnd          0.123            21.1%              8 samples
// So RndSt holds twice as often and twice as long, which is the distinction the two icons draw.
//
// DO NOT READ THESE OFF A PLOT. Eyeballing 10 ms windows of the two gave the OPPOSITE answer at one
// point, because the two captures autoscale to different vertical ranges and the eye compares shapes
// rather than hold times. The statistic is trustworthy where the picture is not.
//
// Rnd IS NOT A PURE GLIDE, though — it still holds 21% of the time, so it is more likely a slewed or
// smoothed random than the linear interpolation drawn here. That refinement is open; the direction it
// differs from RndSt in is not.
#define LFO_RANDOM_STEPS    (6)

static double lfo_random_sample(double phase, bool stepped) {
    static const double levels[LFO_RANDOM_STEPS] = {0.55, -0.30, 0.90, -0.75, 0.20, -0.60};
    double              scaled                   = phase * (double)LFO_RANDOM_STEPS;
    uint32_t            step                     = (uint32_t)scaled;
    double              within                   = scaled - (double)step;

    if (step >= LFO_RANDOM_STEPS) {
        step   = LFO_RANDOM_STEPS - 1;
        within = 1.0;
    }

    if (stepped == true) {
        return levels[step];
    }
    // Glides to the NEXT level, wrapping at the end so the drawn cycle closes on itself rather than
    // stepping across the seam — the same rule the seam logic in render_wave_icon() applies.
    return levels[step] + (within * (levels[(step + 1) % LFO_RANDOM_STEPS] - levels[step]));
}

// LfoA keeps its waveform in a parameter (index 4) and LfoC in a mode (index 0); the wave itself is
// the same either way, which is why one function serves both.
static double lfoa_waveform_sample(uint32_t waveformIndex, double phase) {
    switch (waveformIndex) {
        case 4:
            return lfo_random_sample(phase, true);      // RndSt — held between steps

        case 5:
            return lfo_random_sample(phase, false);     // Rnd — glides between the same values

        default:
            return lfob_waveform_sample(waveformIndex, phase);   // Sin, Tri, Saw, Squ — measured
    }
}

// OscA's family — shapeOscATypeStrMap: Sine, Tri, Saw, Sqr50, Sqr25, Sqr10 — shared by OscA, OscC
// and OscD, which is why one function serves three modules. None of the three has a Shape dial, so
// like LfoB each selection is one static shape.
//
// THE THREE SQUARES ARE PULSE WIDTHS AND THE NAMES SAY SO: Sqr50 is the square, Sqr25 a quarter-cycle
// pulse, Sqr10 a tenth. That is the one part of this set that needs no interpretation at all.
//
// MEASURED ON THE HARDWARE 2026-08-24, on OscA at E4 and again three octaves down, and both of the
// things that were transferred from other modules on the first pass turned out to need correcting:
//   - THE SAW RISES. Drawn falling (carried over from LfoB, whose saw genuinely does fall) it scored
//     0.516; rising, 0.988.
//   - "SQR10" IS NOT A 10% PULSE. Fitting ideal pulses of every width against the captured cycle puts
//     it at 1/16 — 6.25% — where 10% scores 0.79 and 6.25% scores 0.99. Measured twice, at 145 and at
//     1165 samples per cycle, agreeing to within the fit's own resolution. THE MANUAL SAYS 10% (p174,
//     "Sine, Triangle, Sawtooth, Square, 25% Pulse or 10% Pulse") AND IS WRONG, which by now is the
//     expected outcome rather than a surprise. Its 25% is exact, though, and so is the square's 50%,
//     so the name is only wrong on the one entry — and 1/2, 1/4, 1/16 are all binary fractions, which
//     is what a DSP would be expected to produce.
// The pulses need NO edge ramps: drawn as ideal steps they correlate 0.992 and 0.989, so whatever
// band-limiting OscShpB's Pulse needed (pulse_edge_width() above) does not show here.
// Correlations, rotation-free, against the cycle the instrument produced:
//   Sine 1.0000   Tri 0.9981   Saw 0.9880   Sqr50 0.9917   Sqr25 0.9888   Sqr10 0.9903
static double osc_a_waveform_sample(uint32_t waveformIndex, double phase) {
    switch (waveformIndex) {
        case 0:
            return basic_sine(phase);              // Sine

        case 1:
            return basic_triangle(phase);          // Tri

        case 2:
            return basic_rising_saw(phase);        // Saw — RISES, measured; see above

        case 3:
            return basic_pulse(phase, 0.5000);     // Sqr50 — measured 0.4975

        case 4:
            return basic_pulse(phase, 0.2500);     // Sqr25 — measured 0.2500

        default:
            return basic_pulse(phase, 0.0625);     // Sqr10 — measured 1/16, NOT the manual's 10%
    }
}

// OscB's five — shapeTypeStrMap: Sine, Tri, Saw, Sqr, DualSaw. MEASURED 2026-08-24, and every one of
// them turned out to be a law we already had, so this is an index remap and not a new family.
//
// SHAPE ONLY REACHES TWO OF THE FIVE. Sweeping it across the whole dial leaves Sine a pure sine (no
// harmonic above the first rises above 0.01 at any setting), Tri a triangle and Saw a sawtooth whose
// harmonics stay at 1/n to two decimals. Only Sqr and DualSaw respond to it at all — which is worth
// knowing before anyone models a Shape law for the other three.
//   - SINE, TRI: the plain primitives.
//   - SAW RISES, like OscA's and unlike LfoB's: 0.988 against the rising form, 0.516 against the
//     falling one.
//   - SQR IS OscShpB's PULSE, law and all. Measured duty runs 0.500, 0.375, 0.250, 0.125 at Shape
//     0/32/64/96 — a straight 0.5 - 0.49*shape, which is exactly what oscshpb_waveform_sample() case
//     6 already draws. At the top of the dial it lands on 0.055 rather than continuing to 0.010, and
//     0.055 of a 145.6-sample cycle is EIGHT SAMPLES: the same floor OscShpB's Pulse was measured to
//     have. So the floor belongs to the instrument's pulse generator rather than to one module.
//   - DUALSAW IS OscShpB's DBLSAW, at 0.999 across the dial and clearly separated from every runner-up
//     (0.87 at best). Its Shape is the same detune.
// THE MANUAL MISCOUNTS THIS MODULE (p174): it says "one of five waveforms" and then lists six, adding
// a symmetric pulse. There is no sixth — writing 5 to the waveform parameter gives a cycle identical
// to DualSaw at 1.000, so the instrument clamps and paramLocationList's declared range of 5 is right.
static double osc_b_waveform_sample(uint32_t waveformIndex, double phase, double shape) {
    switch (waveformIndex) {
        case 0:
            return basic_sine(phase);                            // Sine — Shape does nothing

        case 1:
            return basic_triangle(phase);                        // Tri — Shape does nothing

        case 2:
            return basic_rising_saw(phase);                      // Saw — Shape does nothing

        case 3:
            return oscshpb_waveform_sample(6, phase, shape);     // Sqr — OscShpB's Pulse

        default:
            return oscshpb_waveform_sample(5, phase, shape);     // DualSaw — OscShpB's DblSaw
    }
}

// Which parameter, on which module, is the waveform picker whose button should show a PICTURE. Kept
// as a list in one place rather than spread through the render code, so adding a module is one line.
// OscShpB keeps its waveform in a MODE rather than a parameter, so it reaches the button through
// render_mode_common() instead of the parameter path — which is why it was the last picker still
// showing words after the other three were converted.
bool module_wave_picker_mode(uint32_t moduleType, uint32_t modeIndex) {
    return ((moduleType == moduleTypeOscShpB) && (modeIndex == 0))
           || ((moduleType == moduleTypeOscC) && (modeIndex == 0))
           || ((moduleType == moduleTypeOscD) && (modeIndex == 0))
           || ((moduleType == moduleTypeLfoC) && (modeIndex == 0));
}

bool module_wave_picker_param(uint32_t moduleType, uint32_t paramIndex) {
    return ((moduleType == moduleTypeOscShpA) && (paramIndex == 9))
           || ((moduleType == moduleTypeLfoShpA) && (paramIndex == 11))
           || ((moduleType == moduleTypeLfoB) && (paramIndex == 4))
           || ((moduleType == moduleTypeOscA) && (paramIndex == 4))
           || ((moduleType == moduleTypeOscB) && (paramIndex == 8))
           || ((moduleType == moduleTypeLfoA) && (paramIndex == 4))
           || ((moduleType == moduleTypeRect) && (paramIndex == 0))
           || ((moduleType == moduleTypeShpStatic) && (paramIndex == 0));
}

// One sample of whichever wave family the module in hand belongs to. The four waveform pickers reach
// three different sample functions and keep their waveform in three different places, so the choice
// is made once here rather than at each call site.
static double module_wave_sample(uint32_t moduleType, uint32_t waveValue, double phase, double shape) {
    if (moduleType == moduleTypeLfoB) {
        return lfob_waveform_sample(waveValue, phase);
    }

    // OscA, OscC and OscD share one waveform set, and it reaches them by two different routes: OscA
    // keeps it in a parameter, the other two in a mode. That difference belongs to the pickers, not
    // to the wave, so all three arrive here together.
    if ((moduleType == moduleTypeOscA) || (moduleType == moduleTypeOscC) || (moduleType == moduleTypeOscD)) {
        return osc_a_waveform_sample(waveValue, phase);
    }

    if (moduleType == moduleTypeLfoShpA) {
        return lfoshpa_waveform_sample(waveValue, phase, shape);
    }

    if (moduleType == moduleTypeOscB) {
        return osc_b_waveform_sample(waveValue, phase, shape);
    }

    if ((moduleType == moduleTypeLfoA) || (moduleType == moduleTypeLfoC)) {
        return lfoa_waveform_sample(waveValue, phase);
    }

    // The box's horizontal axis is the INPUT for these two, not phase — see shaper_transfer_sample().
    if (module_wave_is_transfer(moduleType) == true) {
        return shaper_transfer_sample(moduleType, waveValue, (phase * 2.0) - 1.0);
    }

    if (moduleType == moduleTypeOscShpA) {
        static const uint32_t shpAToShpB[] = {0, 1, 2, 3, 4, 7};

        if (waveValue >= (sizeof(shpAToShpB) / sizeof(shpAToShpB[0]))) {
            waveValue = 0;
        }
        return oscshpb_waveform_sample(shpAToShpB[waveValue], phase, shape);
    }
    return oscshpb_waveform_sample(waveValue, phase, shape);
}

// WHERE EACH MODULE KEEPS ITS WAVEFORM, AND THE SHAPE THAT GOES WITH IT. The picker predicates above
// name the INDEX; these return the VALUE, so the small icon and the big graph ask the same question in
// the same way. This used to be written out twice — once for the icon and once inside the graph
// renderer, as a chain of isLfoB / isLfoShpA / isShpA tests — and adding OscB would have made it a
// third copy of the same knowledge.
static uint32_t module_wave_value(tModule * module, uint32_t variation) {
    switch (module->type) {
        case moduleTypeOscShpB:                     // a MODE on these, a parameter on the rest
        case moduleTypeOscC:
        case moduleTypeOscD:
        case moduleTypeLfoC:
            return module->mode[0].value;

        case moduleTypeOscShpA:
            return module->param[variation][9].value;

        case moduleTypeLfoShpA:
            return module->param[variation][11].value;

        case moduleTypeLfoB:
        case moduleTypeOscA:
        case moduleTypeLfoA:
            return module->param[variation][4].value;

        case moduleTypeOscB:
            return module->param[variation][8].value;

        case moduleTypeRect:                        // a transfer curve, not a wave — see
        case moduleTypeShpStatic:                   // shaper_transfer_sample()
            return module->param[variation][0].value;

        default:
            return 0;
    }
}

// 0..1, or the icons' fixed stand-in for a module that has no Shape dial at all. The sample functions
// for those modules ignore the argument, so the value only has to be harmless.
static double module_shape_value(tModule * module, uint32_t variation) {
    uint32_t index = 0;

    switch (module->type) {
        case moduleTypeOscShpB:
        case moduleTypeOscB:
            index = 6;
            break;

        case moduleTypeOscShpA:
            index = 7;
            break;

        case moduleTypeLfoShpA:
            index = 5;
            break;

        default:
            return WAVE_ICON_FIXED_SHAPE;           // LfoB, OscA, OscC, OscD — no Shape dial
    }
    return (double)module->param[variation][index].value / 127.0;
}

// THE ORIGINAL EDITOR PICKS WAVEFORMS WITH PICTURES, NOT WORDS — its selector buttons carry little
// line drawings of the wave. This draws the same idea in our own style rather than lifting its
// bitmaps: the pixels are in its resource file and decode cleanly, but they are Clavia's artwork, and
// we can do better than copy them anyway. Every one of these waves now has a MEASURED model behind
// it, so the icon is generated from the same function that draws the module's big graph — which makes
// it resolution independent, themed like everything else, and automatically correct if a law is ever
// refined. (CT agreed this approach 2026-08-23.)
double module_wave_icon_shape(uint32_t moduleType) {
    // See the note at the picker's render site: a FIXED shape, chosen so the waves are told apart.
    return (moduleType == moduleTypeLfoShpA) ? WAVE_ICON_LFOSHPA_SHAPE : WAVE_ICON_FIXED_SHAPE;
}

void render_wave_icon(tRectangle buttonRect, uint32_t moduleType, uint32_t waveValue, double shape) {
    const int    numSamples = 48;   // enough for a 30-pixel-wide button; the big graph uses 200
    const double inset      = 2.0;
    double       left       = buttonRect.coord.x + inset;
    double       width      = buttonRect.size.w - (2.0 * inset);
    double       midY       = buttonRect.coord.y + (buttonRect.size.h / 2.0);
    double       halfHeight = (buttonRect.size.h / 2.0) - inset;
    tCoord       previous   = {0};
    tCoord       first      = {0};
    // The face is already zoomed, so the trace's own width has to be too, or it stays hairline-thin
    // when the canvas is zoomed in and coarse when zoomed out.
    double       thickness  = fmax(1.0, buttonRect.size.h / 12.0);

    if ((width <= 0.0) || (halfHeight <= 0.0)) {
        return;
    }

    for (int i = 0; i <= numSamples; i++) {
        double xFraction = (double)i / (double)numSamples;
        double sample    = module_wave_sample(moduleType, waveValue, xFraction, shape);
        tCoord point     = {left + (xFraction * width), midY - (sample * halfHeight)};

        if (i == 0) {
            first = point;
        } else {
            // mainArea, NOT moduleArea, and that is the whole point. render_line() applies the
            // canvas's zoom and scroll itself when it is given moduleArea — but the rectangle this
            // draws into came back from the button renderer with that transform ALREADY applied, so
            // asking for it again placed the icon at neither the right size nor the right position
            // and left it standing still while the canvas moved under it. mainArea skips the second
            // adjustment and keeps the global scaling both paths share. (Same trap as the radio
            // buttons' click regions earlier: draw_button returns ADJUSTED coordinates.)
            render_line(mainArea, previous, point, thickness);
        }
        previous = point;
    }

    // CLOSE THE SEAM ONLY WHERE THE WAVE ACTUALLY JUMPS. A single cycle drawn on its own leaves its
    // two ends unjoined, so a saw came out as a bare diagonal with nothing to show the flyback that
    // makes it a saw (CT). The first attempt dropped a vertical to the ZERO LINE at each end, which
    // fixed the saw and broke the triangle: Saw2Tri starts and ends at -1, so it grew a half-height
    // stub at both ends that is not part of the wave (CT again).
    //
    // The seam is a WRAP, so the honest thing to draw is the step across it — from where the cycle
    // ends to where it begins — and only when there is a step to draw. A triangle ends where it
    // started, so nothing is drawn and it closes on its own; a saw or a square ends a full swing
    // away from its start, and gets the single vertical edge that identifies it.
    if (  (module_wave_is_transfer(moduleType) == false)
       && (fabs(previous.y - first.y) > (buttonRect.size.h * 0.05))) {
        // DRAWN AT BOTH ENDS, because the wave repeats: the step across the seam is the same edge
        // whether you meet it leaving one cycle or entering the next, and showing it only on the
        // right left the saw and the square looking like they began in mid-air (CT). With both, one
        // cycle reads as a complete, bounded waveform.
        render_line(mainArea, previous, (tCoord){previous.x, first.y}, thickness);
        render_line(mainArea, first, (tCoord){first.x, previous.y}, thickness);
    }
}

static void render_oscshpb_waveform_graph(tRectangle rectangle, tModule * module) {
    // ONE PERIOD OF WHATEVER WAVE THIS MODULE IS SET TO, green on grey, in the box graphLocationList
    // gives it. Four modules share this renderer — OscShpB, OscShpA, LfoB and OscB — and they keep
    // their waveform in three different places and their Shape in three more. None of that is decided
    // here any more: module_wave_value(), module_shape_value() and module_wave_sample() answer those
    // three questions for every module, so this function is only the drawing.
    // OSCSHPA SHARES THIS RENDERER, AND ITS WAVES ARE THE SAME WAVES. Measured 2026-08-23: each of
    // OscShpA's six correlates 0.990-0.999 with one of OscShpB's laws and is clearly separated from
    // the runner-up, so the same sample function serves both. What differs is only how the module
    // says which wave and how shaped:
    //   - OscShpB keeps the waveform in a MODE (its only one) and Shape at param 6.
    //   - OSCSHPA HAS NO MODES AT ALL — the instrument answers "module has 0" when asked — so its
    //     waveform is a genuine parameter, index 9, with Shape at 7. That difference was the open
    //     question when this was planned; the hardware settled it, and paramLocationList was right.
    //   - OscShpA offers SIX waves, not eight: it drops DblSaw and Pulse, so its index 5 is
    //     OscShpB's SymPulse (7) and the first five map straight across.
    uint32_t               slot          = module->key.slot;
    uint32_t               variation     = gPatchDescr[slot].activeVariation;
    uint32_t               waveformValue = module_wave_value(module, variation);
    double                 shape         = module_shape_value(module, variation);
    const tGraphLocation * graphLoc      = find_graph_location(module->type);
    tRectangle             graphRect     = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 midY          = graphRect.coord.y + (graphRect.size.h / 2.0);
    const int              numSamples    = 200;     // fine enough to resolve Pulse/SymPulse's narrow
                                                    // sub-sample-width edge ramps, not just the coarser
                                                    // per-cycle shapes
    const int              numCycles     = 1;       // one period across the box, matching the original editor
    tCoord                 prev          = {0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, midY}, (tCoord){graphRect.coord.x + graphRect.size.w, midY}, 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    tCoord                 firstPoint    = {0};
    double                 preWrapY      = 0.0;

    for (int i = 0; i <= numSamples; i++) {
        double xFraction = (double)i / (double)numSamples;                 // raw position across the box, 0..1
        double phase     = fmod(xFraction * numCycles, 1.0);               // wrapped per-cycle phase for the sample
        // THE SAME CALL THE ICON MAKES. Which family a module belongs to, and any index remapping
        // between families, is decided once in module_wave_sample() — so a law corrected there
        // corrects the big graph and the little button together, and a module added there gets both.
        double sample    = module_wave_sample(module->type, waveformValue, phase, shape);
        tCoord point     = {
            graphRect.coord.x + (xFraction * graphRect.size.w),
            graphRect.coord.y + (graphRect.size.h / 2.0) - (sample * graphRect.size.h * 0.45)
        };

        if (i == 0) {
            firstPoint = point;
        } else {
            render_line(moduleArea, prev, point, 1.5);
        }

        if (i == numSamples) {
            preWrapY = prev.y;      // the sample BEFORE the trace wraps back to where it began
        }
        prev = point;
    }

    // THE RIGHT-HAND EDGE DRAWS ITSELF, THE LEFT-HAND ONE DOES NOT. The sweep runs to xFraction 1.0,
    // where fmod() wraps the phase back to 0 — so the final segment leaps from the end of the cycle
    // to its start and paints the flyback at the right of the box. Nothing precedes the first sample,
    // so the same edge is missing on the left, and a saw sat in its box bounded on one side only
    // (CT). Mirroring it there completes the shape.
    //
    // Gated on a real step, exactly as the picker's icon is: a wave that ends where it began — a
    // triangle, a sine, a bell — must NOT be given a vertical, or it grows a stub that is not part of
    // the wave.
    if (fabs(preWrapY - firstPoint.y) > (graphRect.size.h * 0.05)) {
        render_line(moduleArea, firstPoint, (tCoord){firstPoint.x, preWrapY}, 1.5);
    }
}

// ── EnvADSR envelope preview ─────────────────────────────────────────────────
//
// A small live graph of the classic Attack/Decay/Sustain/Release envelope shape, same spirit
// and roughly the same size as the OscShpB waveform preview above. Segment WIDTHS are drawn
// from each knob's own raw value independently (not real time - Attack/Decay/Release span
// 0.5ms to 45s each, far too wide a dynamic range to draw to scale in a
// small box; and not normalised against each other either, since dividing by the sum of all
// three made each segment's width depend on the OTHER two as well as its own knob, saturating
// quickly enough that sweeping one knob only looked like it had two states). The Sustain
// plateau's HEIGHT reflects its level directly (it's a level, not a timed phase, so it gets a
// fixed display width just to show the hold clearly).
//
// Both Env Shape and Output Type are taken from the manual's "COMMON ENVELOPE GENERATOR
// PARAMETERS" section (g2manual.txt), not guesses:
//
// - SHAPE SCROLL BUTTON (envShapeStrMap - "there are four alternatives: Logarithmic Attack &
//   Exponential Decay/Release, Linear Attack & Exponential Decay/Release, Exponential Attack &
//   Decay/Release and Linear Attack & Decay/Release") - so the first word of each is Attack's
//   curve, the second is Decay+Release's. Log (the default) is concave (fast then levelling -
//   confirmed against the real original editor); Exp attack is its mirror, convex (slow then a
//   fast final approach); Decay/Release's Exp is concave, matching a capacitor discharging.
// - OUTPUT TYPE SCROLL BUTTON ("Pos: 0 up to +64 then down to 0. PosInv: +64 down to 0 then up
//   to +64, i.e. inverted. Neg/NegInv mirror Pos/PosInv into negative range. Bip/BipInv: bipolar,
//   sustain level fixed at 0 (ignoring the Sustain knob)"). Implemented as one shape computed in
//   Pos's own convention, then optionally reflected (1-x, for PosInv/Neg) and/or negated (for
//   Neg/NegInv/BipInv) - Bip/BipInv additionally swap in a fixed 0 sustain and let Release run
//   on to -1 instead of stopping at 0.
// - GRAPHS ("Any sustain level is indicated with an orange line; the rest...are green. There is
//   also a yellow horizontal line which indicates the zero level") - colours below match this
//   directly.
// The attack and decay/release curves themselves are paramCurves.c's, shared with the sound engine
// so the envelope drawn here is the one that is actually played. This file keeps only the part that
// is a DRAWING concern: a segment on the face runs between two arbitrary levels - full down to
// Sustain, Sustain down to the release target - where the engine's own segments always run 1 to 0.
//
// Attack curve types (envShapeStrMap's first word): 0=Log (default), 1=Lin, 2=Exp, 3=Lin.
// Decay/Release curve types (its second word): 0-2=Exp, 3=Lin - which env_fall_level() applies.
static double envadsr_decay_level(double t, double levelStart, double levelEnd, uint32_t envShapeIndex) {
    return levelEnd + ((levelStart - levelEnd) * env_fall_level(envShapeIndex, t));
}

// Maps an envelope "shape" value (0 at the start, 1 at the attack peak) to a y coordinate.
// "shape" is always expressed in Pos's own convention; each Output Type converts it to what it
// actually outputs (per the manual's six descriptions) before the mapping.
static double env_level_to_y(double shape, uint32_t outputType, double zeroY, double fullSwing) {
    double actualLevel;

    switch (outputType) {
        case 1: actualLevel  = 1.0 - shape;
            break;                                   // PosInv

        case 2: actualLevel  = shape - 1.0;
            break;                                   // Neg

        case 3: actualLevel  = -shape;
            break;                                   // NegInv

        case 5: actualLevel  = -shape;
            break;                                   // BipInv

        default: actualLevel = shape;
            break;                                   // Pos, Bip
    }
    return zeroY - (actualLevel * fullSwing);
}

static void render_envadsr_graph(tRectangle rectangle, tModule * module) {
    // Env Shape (index 0), Attack (1), Decay (2), Sustain (3), Release (4), Output Type (5) -
    // fixed positions for moduleTypeEnvADSR's entries in paramLocationList, see moduleResources.h.
    const uint32_t         envShapeParamIndex   = 0;
    const uint32_t         attackParamIndex     = 1;
    const uint32_t         decayParamIndex      = 2;
    const uint32_t         sustainParamIndex    = 3;
    const uint32_t         releaseParamIndex    = 4;
    const uint32_t         outputTypeParamIndex = 5;
    uint32_t               slot                 = module->key.slot;
    uint32_t               variation            = gPatchDescr[slot].activeVariation;
    uint32_t               envShapeIndex        = module->param[variation][envShapeParamIndex].value;
    double                 attackVal            = (double)module->param[variation][attackParamIndex].value / 127.0;
    double                 decayVal             = (double)module->param[variation][decayParamIndex].value / 127.0;
    double                 sustainLevel         = (double)module->param[variation][sustainParamIndex].value / 127.0;
    double                 releaseVal           = (double)module->param[variation][releaseParamIndex].value / 127.0;
    uint32_t               outputType           = module->param[variation][outputTypeParamIndex].value; // 0=Pos,
                                                                                                        // 1=PosInv, 2=Neg,
                                                                                                        // 3=NegInv, 4=Bip,
                                                                                                        // 5=BipInv
    bool                   isBip                = (outputType == 4) || (outputType == 5);
    bool                   isNegFamily          = (outputType == 2) || (outputType == 3);

    // Bip/BipInv ignore the Sustain knob (fixed at the centre level instead) and Release
    // continues on past that centre to the opposite extreme, rather than stopping there.
    double                 effectiveSustain     = isBip ? 0.0 : sustainLevel;
    double                 releaseTarget        = isBip ? -1.0 : 0.0;

    // Centred horizontally, vertically aligned with the KB Active toggle's own y offset (8).
    const tGraphLocation * graphLoc             = find_graph_location(module->type);
    tRectangle             graphRect            = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);

    const double           holdWidth            = 0.24; // fixed width just to show the Sustain plateau clearly

    // Each segment's width comes from its OWN knob only, independent of the other two - 0.04
    // (raw minimum, still clearly visible rather than a zero-width vertical line) up to 0.24
    // (raw maximum). Whatever's left of the box after Attack+Decay+Release+the fixed Sustain
    // width is just background.
    double                 attackW              = 0.04 + (attackVal * 0.20);
    double                 decayW               = 0.04 + (decayVal * 0.20);
    double                 releaseW             = 0.04 + (releaseVal * 0.20);
    double                 x0                   = graphRect.coord.x;

    // Where "envelope output = 0" sits, and how much of the box height its full swing covers -
    // Pos/PosInv only ever go 0..+1 (manual: "0 units...up to +64"), so zero sits at the
    // bottom; Neg/NegInv only ever go 0..-1, so zero sits at the top; only Bip/BipInv are
    // genuinely bipolar, spanning -1..+1 around a centred zero.
    double                 zeroY                = isBip ? (graphRect.coord.y + (graphRect.size.h * 0.5)) : (isNegFamily ? graphRect.coord.y : (graphRect.coord.y + graphRect.size.h));
    double                 fullSwing            = isBip ? (graphRect.size.h * 0.5) : graphRect.size.h;

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_YELLOW_7);
    render_line(moduleArea, (tCoord){x0, zeroY}, (tCoord){x0 + graphRect.size.w, zeroY}, 1.0);

    // "shape" (0 at the start, 1 at the attack peak, effectiveSustain during hold, releaseTarget
    // at the end) is always expressed in Pos's own convention; convert it to what each Output
    // Type actually outputs (per the manual's six descriptions) before mapping to a y coordinate.

    tCoord                 p0                   = {x0, env_level_to_y(0.0, outputType, zeroY, fullSwing)};
    tCoord                 p1                   = {x0 + (attackW * graphRect.size.w), env_level_to_y(1.0, outputType, zeroY, fullSwing)};
    tCoord                 p2                   = {p1.x + (decayW * graphRect.size.w), env_level_to_y(effectiveSustain, outputType, zeroY, fullSwing)};
    tCoord                 p3                   = {p2.x + (holdWidth * graphRect.size.w), p2.y};
    tCoord                 p4                   = {p3.x + (releaseW * graphRect.size.w), env_level_to_y(releaseTarget, outputType, zeroY, fullSwing)};

    const int              numCurveSteps        = 12;
    tCoord                 prev                 = p0;

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 1; i <= numCurveSteps; i++) {
        double t     = (double)i / (double)numCurveSteps;
        double level = env_attack_level(envShapeIndex, t);
        tCoord point = {p0.x + (t * (p1.x - p0.x)), env_level_to_y(level, outputType, zeroY, fullSwing)};

        render_line(moduleArea, prev, point, 1.5);
        prev = point;
    }

    for (int i = 1; i <= numCurveSteps; i++) {
        double t     = (double)i / (double)numCurveSteps;
        double level = envadsr_decay_level(t, 1.0, effectiveSustain, envShapeIndex);
        tCoord point = {p1.x + (t * (p2.x - p1.x)), env_level_to_y(level, outputType, zeroY, fullSwing)};

        render_line(moduleArea, prev, point, 1.5);
        prev = point;
    }

    set_rgb_colour((tRgb)RGB_ORANGE_1); // sustain segment - matches the original editor's own colouring
    render_line(moduleArea, p2, p3, 1.5);

    set_rgb_colour((tRgb)RGB_GREEN_ON);
    prev = p3;

    for (int i = 1; i <= numCurveSteps; i++) {
        double t     = (double)i / (double)numCurveSteps;
        double level = envadsr_decay_level(t, effectiveSustain, releaseTarget, envShapeIndex);
        tCoord point = {p3.x + (t * (p4.x - p3.x)), env_level_to_y(level, outputType, zeroY, fullSwing)};

        render_line(moduleArea, prev, point, 1.5);
        prev = point;
    }
}

// ── FltClassic response preview ──────────────────────────────────────────────
//
// A small live graph of the classic lowpass filter's frequency response, using the real 2-pole
// resonant lowpass magnitude formula |H(f)|^2 = 1 / [(1-(f/fc)^2)^2 + (f/(fc*Q))^2] rather than
// stitching together a flat line, a Gaussian "bump" and a separate linear rolloff - passband,
// resonant peak (taller and narrower as Q increases, per the manual's "narrow resonance
// peak...similar to...analog ladder filters") and rolloff all fall out of the one formula as a
// single smooth curve. 18/24 dB/octave are modelled as extra plain one-pole rolloff stages
// cascaded onto the base 2-pole (12dB) response, which also naturally narrows the peak further
// at higher slopes, matching real higher-order filter behaviour. Q itself uses the real 0.5..50
// range from flt_resonance_q(). The
// X axis is ~10 octaves of relative frequency (roughly matching Freq's own real ~14Hz..21kHz
// exponential range) rather than a literal Hz-calibrated Bode plot - the box is too small to be
// literal about that regardless of curve shape. The cutoff is inset into a band across the box
// (not mapped edge-to-edge) so the resonance peak always keeps headroom either side - see the
// cutoff calc below.
static void render_fltclassic_response_graph(tRectangle rectangle, tModule * module) {
    // Freq (index 0), Res (index 3), dB/octave slope (index 4) - fixed positions for
    // moduleTypeFltClassic's entries in paramLocationList, see moduleResources.h.
    const uint32_t         freqParamIndex  = 0;
    const uint32_t         resParamIndex   = 3;
    const uint32_t         slopeParamIndex = 4;
    uint32_t               slot            = module->key.slot;
    uint32_t               variation       = gPatchDescr[slot].activeVariation;
    double                 cutoffKnob      = (double)module->param[variation][freqParamIndex].value / 127.0;

    // Inset the cutoff's on-screen position into a band rather than mapping the knob edge-to-edge, so
    // the resonance peak (and the passband/left flank below it) always keeps horizontal headroom. At
    // min Freq the peak used to sit hard against the left edge with its lower half off-screen. This is
    // a small horizontal zoom-out: the box now shows a slightly wider window than the knob's own
    // ~14Hz..21kHz range so the curve never runs off either edge.
    const double           kCutoffMinX     = 0.15;                                            // min-Freq cutoff sits 15% in from the left edge
    const double           kCutoffMaxX     = 0.90;                                            // max-Freq cutoff sits 10% in from the right edge
    double                 cutoffX         = kCutoffMinX + (cutoffKnob * (kCutoffMaxX - kCutoffMinX));
    uint32_t               slopeIndex      = module->param[variation][slopeParamIndex].value; // 0=12dB, 1=18dB, 2=24dB

    // Shared with the dial text and the sound engine — see renderParams.h. What the curve draws and
    // what the engine plays come from one definition.
    //
    // MEASURED, NOT ASSUMED (2026-08-24). This used to draw a BIQUAD with extra one-poles bolted on,
    // and a biquad's gain at DC is 1 whatever its Q — so the drawn passband stayed pinned at 0 dB and
    // only the peak grew. The instrument does the opposite: winding Res up pulls the whole passband
    // DOWN, about 14 dB by the top of the dial, because the resonance is feedback around the poles
    // rather than a Q inside a biquad. See flt_ladder_feedback() in paramCurves.c for the capture and
    // for the part that took the longest to see — the loop is always four poles long and the dB
    // switch only moves the output tap.
    double                 feedback        = flt_ladder_feedback((double)module->param[variation][resParamIndex].value);
    uint32_t               tap             = flt_ladder_tap(slopeIndex);                       // 2/3/4 poles: 12/18/24 dB

    const tGraphLocation * graphLoc        = find_graph_location(module->type);
    tRectangle             graphRect       = adjust_rectangle(rectangle, graphLoc->rectangle, graphLoc->anchor, module);
    double                 baseY           = graphRect.coord.y + (graphRect.size.h * 0.6); // 0dB reference, leaving
                                                                                           // headroom above for the
                                                                                           // resonance peak to rise into
    const int              numSamples      = 100;
    tCoord                 prev            = {0};

    set_rgb_colour((tRgb)RGB_GREY_2);
    render_rectangle(moduleArea, graphRect);

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_line(moduleArea, (tCoord){graphRect.coord.x, baseY}, (tCoord){graphRect.coord.x + graphRect.size.w, baseY}, 1.0);

    set_rgb_colour((tRgb)RGB_GREEN_ON);

    for (int i = 0; i <= numSamples; i++) {
        double x         = (double)i / (double)numSamples;
        double octaves   = (x - cutoffX) * 10.0;   // ~10 octaves span the box, matching Freq's own real range
        double ratio     = pow(2.0, octaves);      // f/fc
        double magnitude = flt_ladder_magnitude(ratio, feedback, tap);
        double levelDb   = 20.0 * log10(fmax(magnitude, 1e-4));
        double level     = fmax(-1.0, fmin(1.0, levelDb / 24.0)); // +-24dB fills the box vertically

        // Scale must stay within min(baseY's own fraction, 1-that fraction) - 0.6/0.55 didn't
        // (0.6+0.55 = 1.15), which is exactly why the rolloff (level -> -1) was plotting below
        // the box's bottom edge; 0.38 fits both the 0.6-above and 0.4-below headroom baseY
        // leaves either side of it.
        tCoord point     = {graphRect.coord.x + (x * graphRect.size.w), baseY - (level * graphRect.size.h * 0.38)};

        if (i > 0) {
            render_line(moduleArea, prev, point, 1.5);
        }

        if (level <= -1.0) {
            break; // fully rolled off - stop rather than trailing a flat line along the bottom edge
        }
        prev = point;
    }
}

void render_module_common(tRectangle rectangle, tModule * module) {
    if (module == NULL) {
        return;
    }
    uint32_t param  = 0;
    uint32_t mode   = 0;
    uint32_t volume = 0;
    uint32_t led    = 0;

    for (uint32_t i = module->paramIndexCache; i < array_size_param_location_list(); i++) {
        if (paramLocationList[i].moduleType == module->type) {
            if (module->gotParamIndexCache == false) {
                module->paramIndexCache    = i;
                module->gotParamIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, paramLocationList[i].rectangle, paramLocationList[i].anchor, module);
            render_param_common(adjusted, module, i, param++);

            if (param >= module_param_count(module->type)) {
                break;
            }
        }
    }

    for (uint32_t i = module->modeIndexCache; i < array_size_mode_location_list(); i++) {
        if (modeLocationList[i].moduleType == module->type) {
            if (module->gotModeIndexCache == false) {
                module->modeIndexCache    = i;
                module->gotModeIndexCache = true;
            }
            //render_mode_common(
            //    {rectangle.coord.x + x_param_pos_from_percent(modeLocationList[i].offsetX), rectangle.coord.y + y_param_pos_from_percent(module->type, modeLocationList[i].offsetY)}, //module, i,
            //    mode++);
            tRectangle adjusted = adjust_rectangle(rectangle, modeLocationList[i].rectangle, modeLocationList[i].anchor, module);
            render_mode_common(adjusted, module, i, mode++);

            if (mode >= module_mode_count(module->type)) {
                break;
            }
        }
    }

    render_module_connectors(rectangle, module);

    if (  (module->type == moduleTypeOscShpB) || (module->type == moduleTypeOscShpA)
       || (module->type == moduleTypeLfoShpA) || (module->type == moduleTypeLfoB)
       || (module->type == moduleTypeOscB) || (module->type == moduleTypeOscA)) {
        render_oscshpb_waveform_graph(rectangle, module);
    }

    if (module->type == moduleTypeEnvADSR) {
        render_envadsr_graph(rectangle, module);
    }

    if (module->type == moduleTypeFltClassic) {
        render_fltclassic_response_graph(rectangle, module);
    }

    for (uint32_t i = module->volumeIndexCache; i < array_size_volume_location_list(); i++) {
        if (volumeLocationList[i].moduleType == module->type) {
            if (module->gotVolumeIndexCache == false) {
                module->volumeIndexCache    = i;
                module->gotVolumeIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, volumeLocationList[i].rectangle, volumeLocationList[i].anchor, module);
            render_volume_common(adjusted, module, i, volume++);

            if (volume >= module_volume_count(module->type)) {
                break;
            }
        }
    }

    // No index cache for this one, unlike the loops above: displayLocationList has ten entries where
    // paramLocationList has a thousand, so a scan costs nothing and there is no cache to go stale.
    for (uint32_t i = 0; i < array_size_display_location_list(); i++) {
        if (displayLocationList[i].moduleType == module->type) {
            tRectangle adjusted = adjust_rectangle(rectangle, displayLocationList[i].rectangle, displayLocationList[i].anchor, module);

            render_display_common(adjusted, module, i);
        }
    }

    for (uint32_t i = module->ledIndexCache; i < array_size_led_location_list(); i++) {
        if (ledLocationList[i].moduleType == module->type) {
            if (module->gotLedIndexCache == false) {
                module->ledIndexCache    = i;
                module->gotLedIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, ledLocationList[i].rectangle, ledLocationList[i].anchor, module);
            adjusted.size.h = adjusted.size.w; // We want this one to be square
            render_led_common(adjusted, module, i, led++);

            // Rows, not stream slots — see module_led_row_count().
            if (led >= module_led_row_count(module->type)) {
                break;
            }
        }
    }
}

void render_module(tModule * module) {
    double     moduleHeight               = gModuleProperties[module->type].height;
    double     xPos                       = module->column * MODULE_X_SPAN;
    double     yPos                       = module->row * MODULE_Y_SPAN;
    double     xWidth                     = MODULE_WIDTH;
    double     yHeight                    = (moduleHeight * MODULE_Y_SPAN) - MODULE_Y_GAP;
    char       buff[CLAVIA_NAME_SIZE + 1] = {0};
    tRgb       rgb                        = {0};

    tRectangle moduleRectangle            = {{xPos, yPos}, {xWidth, yHeight}};

    rgb                                                                        = gModuleColourMap[module->colour];
    set_rgb_colour(rgb);
    module->rectangle                                                          = render_rectangle_with_border(moduleArea, moduleRectangle);

    sModuleClickCtx[module->key.slot][module->key.location][module->key.index] = (tModuleClickCtx){
        eCanvasWidgetModule, module->key
    };
    register_click_region(module->rectangle, eClickLayerCanvas, module_body_click_handler,
                          &sModuleClickCtx[module->key.slot][module->key.location][module->key.index]);

    if (is_selected(module->key)) {
        double t = 2.0;
        double x = moduleRectangle.coord.x;
        double y = moduleRectangle.coord.y;
        double w = moduleRectangle.size.w;
        double h = moduleRectangle.size.h;

        set_rgb_colour((tRgb)RGB_YELLOW_7);
        render_line(moduleArea, (tCoord){x, y}, (tCoord){x + w, y}, t);                 // top
        render_line(moduleArea, (tCoord){x + w, y}, (tCoord){x + w, y + h}, t);         // right
        render_line(moduleArea, (tCoord){x + w, y + h}, (tCoord){x, y + h}, t);         // bottom
        render_line(moduleArea, (tCoord){x, y + h}, (tCoord){x, y}, t);                 // left
    }

    // Patch Mutator: mark excluded modules with a thin red frame, but only while the panel is
    // open - matches the original editor's SetMutaLockVisible (pure display state, not persisted).
    if (gMutator.active && module->excludeFromMutation) {
        double t = 1.0;
        double x = moduleRectangle.coord.x;
        double y = moduleRectangle.coord.y;
        double w = moduleRectangle.size.w;
        double h = moduleRectangle.size.h;

        set_rgb_colour((tRgb)RGB_RED_7);
        render_line(moduleArea, (tCoord){x, y}, (tCoord){x + w, y}, t);                 // top
        render_line(moduleArea, (tCoord){x + w, y}, (tCoord){x + w, y + h}, t);         // right
        render_line(moduleArea, (tCoord){x + w, y + h}, (tCoord){x, y + h}, t);         // bottom
        render_line(moduleArea, (tCoord){x, y + h}, (tCoord){x, y}, t);                 // left
    }
    rgb              = (tRgb){
        rgb.red * 1.05, rgb.green * 1.05, rgb.blue * 1.05
    };
    set_rgb_colour(rgb);
    module->dragArea = render_rectangle(moduleArea, (tRectangle){{moduleRectangle.coord.x + 3, moduleRectangle.coord.y + 3}, {moduleRectangle.size.w - 6, STANDARD_TEXT_HEIGHT + 2}});
    register_click_region(module->dragArea, eClickLayerCanvas, drag_area_click_handler,
                          &sModuleClickCtx[module->key.slot][module->key.location][module->key.index]);

    render_module_common(moduleRectangle, module);

    if (  gModuleNameEdit.active
       && gModuleNameEdit.moduleKey.slot == module->key.slot
       && gModuleNameEdit.moduleKey.location == module->key.location
       && gModuleNameEdit.moduleKey.index == module->key.index) {
        char     editBuf[CLAVIA_NAME_SIZE + 2] = {0};
        uint32_t cp                            = gModuleNameEdit.cursorPos;
        memcpy(editBuf, gModuleNameEdit.buffer, cp);
        editBuf[cp] = '|';
        memcpy(&editBuf[cp + 1], &gModuleNameEdit.buffer[cp], strlen(gModuleNameEdit.buffer) - cp + 1);

        // Highlight the drag area to show edit mode
        set_rgb_colour((tRgb)RGB_WHITE);
        render_rectangle(moduleArea, (tRectangle){{moduleRectangle.coord.x + 3, moduleRectangle.coord.y + 3},
                                                  {get_text_width(LONGEST_MODULE_NAME, STANDARD_BUTTON_TEXT_HEIGHT, eCache) + 5, STANDARD_BUTTON_TEXT_HEIGHT + 2}
                         });

        set_rgba_colour((tRgba)RGBA_BLACK_ON_TRANSPARENT);
        render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0},
                                             {BLANK_SIZE, STANDARD_TEXT_HEIGHT}
                    }, editBuf);
    } else {
        // COPY_STRING, NOT snprintf: the USB thread writes module names as patch data arrives, and
        // this read is on the render thread. Both go through gStringCopyMutex, so what is drawn is
        // one name rather than a mixture of the old and the new. See defs.h.
        COPY_STRING(buff, module->name);
        set_rgba_colour((tRgba)RGBA_BLACK_ON_TRANSPARENT);
        render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0},
                                             {BLANK_SIZE, STANDARD_TEXT_HEIGHT}
                    }, buff);
    }
    // Temporary items purely for development debug
    snprintf(buff, sizeof(buff), "(%s)", gModuleProperties[module->type].name);

    render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + 180.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

    // THE MODULE'S INDEX, top right — a development aid, not something a user has any use for, so it
    // is compiled out of a Release build. The type name above it stays: that one names the module,
    // which is worth having on a face whose title the user may have renamed.
#ifdef DEBUG
    snprintf(buff, sizeof(buff), "%u", module->key.index);
    render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + moduleRectangle.size.w - 20.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
#endif

    // Mode count — debug only, and the one of these three that says nothing a user would want:
    // the type name and the index both identify the module, this just counts its mode entries.
    // Commented rather than deleted so it can go back for protocol work on modes.
    //if (module->modeCount > 0) {
    //    snprintf(buff, sizeof(buff), "Modes %u", module->modeCount);
    //    render_text(moduleArea, (tRectangle){{moduleRectangle.coord.x + 250.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    //}
}

void render_modules(void) {
    uint32_t slot     = gSlot;
    uint32_t location = gLocation;

    // param_overlay_begin_frame() used to live here. It moved out to render_frame() when the canvas
    // gained panes: this function now runs once PER PANE, and resetting the overlay queue between
    // panes would throw away everything the first pane had just queued.

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * module = get_module_slot(slot, location, i);

        if (module->active && module->type != moduleTypeUnknown0) {
            // Skip the (relatively expensive, many-sub-element) render for modules currently
            // scrolled entirely outside the visible canvas. Rect here mirrors render_module()'s
            // own moduleRectangle exactly; rectangle_visible_in_module_area() applies the real
            // scale/scroll transform so a module straddling the viewport edge still renders.
            double     moduleHeight    = gModuleProperties[module->type].height;
            tRectangle moduleRectangle = {{module->column * MODULE_X_SPAN, module->row * MODULE_Y_SPAN                  },
                                          {MODULE_WIDTH,                   (moduleHeight * MODULE_Y_SPAN) - MODULE_Y_GAP}};

            if (!rectangle_visible_in_module_area(moduleRectangle)) {
                // Still off-screen — but cables reference this module's connector positions
                // regardless of whether it's currently visible, so those must stay registered.
                render_module_connectors(moduleRectangle, module);
                continue;
            }
            render_module(module);
        }
    }

    // Only in the pane it was started in. This function runs once per pane now, and the band's
    // coordinates are module-space for ONE pane — drawing it in the other would put a second
    // rectangle at the same module coordinates in the wrong half of the window.
    if (gRubberBand.active && (module_pane() == split_view_focused_pane())) {
        double x1 = gRubberBand.start.x < gRubberBand.current.x ? gRubberBand.start.x : gRubberBand.current.x;
        double y1 = gRubberBand.start.y < gRubberBand.current.y ? gRubberBand.start.y : gRubberBand.current.y;
        double x2 = gRubberBand.start.x > gRubberBand.current.x ? gRubberBand.start.x : gRubberBand.current.x;
        double y2 = gRubberBand.start.y > gRubberBand.current.y ? gRubberBand.start.y : gRubberBand.current.y;

        set_rgb_colour((tRgb)RGB_YELLOW_7);
        render_line(moduleArea, (tCoord){x1, y1}, (tCoord){x2, y1}, 1.5); // top
        render_line(moduleArea, (tCoord){x2, y1}, (tCoord){x2, y2}, 1.5); // right
        render_line(moduleArea, (tCoord){x2, y2}, (tCoord){x1, y2}, 1.5); // bottom
        render_line(moduleArea, (tCoord){x1, y2}, (tCoord){x1, y1}, 1.5); // left
    }
    // Draw background areas
    //set_rgb_colour((tRgb)RGB_RED_7/*RGB_BACKGROUND_GREY*/);
    //tRectangle area        = module_area();
    //render_rectangle(mainArea, (tRectangle){{0.0, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    //render_rectangle(mainArea, (tRectangle){{0.0, area.coord.y - MODULE_MARGIN}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
    //render_rectangle(mainArea, (tRectangle){{area.coord.x + area.size.w, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    //render_rectangle(mainArea, (tRectangle){{0.0, area.coord.y + area.size.h}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
}

void render_cable_from_to(tConnector from, tConnector to, double thickness) {
    tCoord control   = {0};

    from.coord.x += scale_from_percent(CONNECTOR_SIZE / 2.0);
    from.coord.y += scale_from_percent(CONNECTOR_SIZE / 2.0);
    to.coord.x   += scale_from_percent(CONNECTOR_SIZE / 2.0);
    to.coord.y   += scale_from_percent(CONNECTOR_SIZE / 2.0);

    double dy        = to.coord.y - from.coord.y;
    double bowAmount = fmin(fabs(dy) * 0.3, 80.0);

    if (from.coord.x == to.coord.x) {
        // Exactly vertical — bow horizontally so the cable is visible
        control.x = fmax(from.coord.x, to.coord.x) + bowAmount;
    } else {
        // All other cables — gravity sag downward
        control.x = (from.coord.x + to.coord.x) / 2.0;
    }
    control.y     = fmax(from.coord.y, to.coord.y) + 40.0;

    // A quadratic bezier curve is always fully contained within the bounding box of its 3
    // control points, so this bbox — built from the endpoints AND the sag point, not just
    // the endpoints — is a correct (if slightly loose) bound on where the curve can actually
    // fall. Skip the draw if none of that box is visible.
    double minX      = fmin(fmin(from.coord.x, to.coord.x), control.x);
    double maxX      = fmax(fmax(from.coord.x, to.coord.x), control.x);
    double minY      = fmin(fmin(from.coord.y, to.coord.y), control.y);
    double maxY      = fmax(fmax(from.coord.y, to.coord.y), control.y);

    if (!rectangle_visible_in_module_area((tRectangle){{minX, minY}, {maxX - minX, maxY - minY}})) {
        return;
    }
    render_bezier_curve(moduleArea, from.coord, control, to.coord, thickness, 15);
}

static bool cable_touches_hover_connector(tCable * cable) {
    if (!gHoverConnector.active) {
        return false;
    }

    if (cable->key.slot != gHoverConnector.slot || cable->key.location != gHoverConnector.location) {
        return false;
    }

    if (  cable->key.moduleFromIndex == gHoverConnector.moduleIndex
       && cable->key.connectorFromIoCount == gHoverConnector.ioCount
       && cable->key.linkType == (uint32_t)gHoverConnector.dir) {
        return true;
    }

    if (  cable->key.moduleToIndex == gHoverConnector.moduleIndex
       && cable->key.connectorToIoCount == gHoverConnector.ioCount
       && gHoverConnector.dir == connectorDirIn) {
        return true;
    }
    return false;
}

void render_cable(tCable * cable, double alpha) {
    tRgb      colour             = gCableColourMap[cable->colour];

    tModule * moduleFrom         = get_module((tModuleKey){cable->key.slot, cable->key.location, cable->key.moduleFromIndex});

    if (moduleFrom == NULL) {
        return;
    }
    tModule * moduleTo           = get_module((tModuleKey){cable->key.slot, cable->key.location, cable->key.moduleToIndex});

    if (moduleTo == NULL) {
        return;
    }

    // No blend enable/disable: blending is on for the whole session (render_backend_init()).
    // The pair that used to sit here was compensating for render_text() disabling it.
    if (alpha < 1.0) {
        set_rgba_colour((tRgba){colour.red, colour.green, colour.blue, alpha});
    } else {
        set_rgb_colour(colour);
    }
    int       fromConnectorIndex = find_index_from_io_count(moduleFrom, (tConnectorDir)cable->key.linkType, cable->key.connectorFromIoCount);

    int       toConnectorIndex   = find_index_from_io_count(moduleTo, connectorDirIn, cable->key.connectorToIoCount);

    if (fromConnectorIndex != -1 && toConnectorIndex != -1) {
        render_cable_from_to(moduleFrom->connector[fromConnectorIndex], moduleTo->connector[toConnectorIndex], 4.0);
    }
}

// The cable a Ctrl-drag has picked up is NOT DRAWN while the drag is in flight. It still exists —
// the delete happens at release, so that the whole re-route is one undo step — but leaving it on
// screen showed the cable in its old position while the rubber band drew the new one, which reads as
// though a second cable is being made rather than this one being moved. The manual's "pull out the
// connector" is what the user should see: the cable leaves its socket and follows the cursor.
static bool cable_is_being_rerouted(const tCable * cable) {
    if (!gCableDrag.active || !gCableDrag.rerouting) {
        return false;
    }
    return cable_touches_connector(cable, gCableDrag.rerouteModuleIndex,
                                   gCableDrag.rerouteIoCount, gCableDrag.rerouteDir);
}

void render_cables(void) {
    uint32_t slot           = gSlot;
    uint32_t location       = gLocation;
    bool     hideAll        = gCablesHideAll;
    bool     allTransparent = gCablesTransparent;
    bool     hoverActive    = gHoverConnector.active;
    double   normalAlpha    = allTransparent ? 0.5 : 1.0;

    if (hideAll) {
        return;
    }

    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable         = get_cable_slot(slot, location, i);

        if (cable == NULL || !cable->active || cable_is_being_rerouted(cable)) {
            continue;
        }
        bool     colourVisible = gPatchDescr[slot].visible[cable->colour];
        bool     isHovered     = cable_touches_hover_connector(cable);

        if (!colourVisible || (hoverActive && isHovered)) {
            continue;
        }
        render_cable(cable, hoverActive ? 0.2 : normalAlpha);
    }

    if (hoverActive) {
        for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
            tCable * cable         = get_cable_slot(slot, location, i);

            if (cable == NULL || !cable->active || cable_is_being_rerouted(cable)) {
                continue;
            }
            bool     colourVisible = gPatchDescr[slot].visible[cable->colour];
            bool     isHovered     = cable_touches_hover_connector(cable);

            if (!colourVisible || !isHovered) {
                continue;
            }
            render_cable(cable, 1.0);
        }
    }
}

// Which source a morph group is taking, given its selector value. Every group offers the knob at 0
// and one named alternative at 1 - except the FIFTH, which has a third: the first global wheel.
// Treating any non-zero value as "the" named source showed a group 5 morph assigned to G.Wh 1 as
// Sust.Pd instead.
#define MORPH_GROUP_WITH_THIRD_SOURCE    (4)     // zero-based, i.e. the fifth group

static const char * morph_source_name(uint32_t group, uint32_t sourceValue) {
    if ((group == MORPH_GROUP_WITH_THIRD_SOURCE) && (sourceValue > 1)) {
        return "G.Wh 1";
    }
    return morphStrMap[group];
}

void render_morph_groups(void) {
    tRectangle rectangle        = {{840, 4 + MENU_BAR_HEIGHT}, {STANDARD_TEXT_HEIGHT *2, STANDARD_TEXT_HEIGHT * 4}};
    char       dialValueStr[16] = {0};
    char       label[16]        = {0};
    tRgb       dialColour       = (tRgb)RGB_BACKGROUND_GREY;
    uint32_t   i                = 0;
    double     textHeight       = 0.0;
    bool       isKnob           = false;
    uint8_t    dialValue        = 0;
    uint32_t   slot             = gSlot;
    uint32_t   variation        = gPatchDescr[slot].activeVariation;

    tModule *  module           = get_module((tModuleKey){slot, (uint32_t)locationMorph, 1});

    if (module != NULL) {
        // The per-frame "nullify all the rectangles so stale ones cannot be clicked" loop that used
        // to be here is gone with the array: hit-testing comes from the click-region registry, and
        // clear_click_regions() empties that at the top of every frame. A widget that is not drawn
        // this frame is not registered this frame, which is the same guarantee without the bookkeeping.

        for (i = 0; i < NUM_MORPHS; i++) {
            isKnob     = !(module->param[variation][i + NUM_MORPHS].value != 0);
            dialValue  = module->param[variation][i].value;

            snprintf(dialValueStr, sizeof(dialValueStr), "%u", dialValue);

            if (isKnob) {
                snprintf(label, sizeof(label), "%s", module->paramName[i + NUM_MORPHS][0]);

                if (label[0] == '\0') {
                    snprintf(label, sizeof(label), "Knob");
                }
            } else {
                snprintf(label, sizeof(label), "%s", morph_source_name(i, module->param[variation][i + NUM_MORPHS].value));
            }
            textHeight = rectangle.size.h / 4.0;

            set_rgb_colour((tRgb)RGB_BLACK);
            render_text(mainArea, (tRectangle){{rectangle.coord.x - 3, rectangle.coord.y}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, (char *)morphStrMap[i]);

            if (i == gMorphGroupFocus) {
                dialColour = isKnob ? (tRgb)RGB_ORANGE_0 : (tRgb)RGB_ORANGE_2;
            } else {
                dialColour = (tRgb)RGB_GREY_3;
            }
            // + textHeight on top of the existing + 16 because render_dial_with_text() is
            // dial-anchored: the rect is now the circle and the value string is drawn in the row
            // above it, where it previously started at the rect's own y.
            tRectangle dialRect = render_dial_with_text(mainArea, (tRectangle){{rectangle.coord.x, rectangle.coord.y + 16 + textHeight}, {rectangle.size.w, rectangle.size.w}}, NULL, dialValueStr, textHeight, module->param[variation][i].value, 128, module->param[variation][i].morphRange[gMorphGroupFocus], dialColour);

            register_click_region(dialRect, eClickLayerPanel, morph_param_click_handler,
                                  morph_click_ctx(module, i));

            if (  gParamNameEdit.active
               && gParamNameEdit.moduleKey.slot == module->key.slot
               && gParamNameEdit.moduleKey.location == module->key.location
               && gParamNameEdit.moduleKey.index == module->key.index
               && gParamNameEdit.paramIndex == i + NUM_MORPHS) {
                char     editBuf[PROTOCOL_PARAM_NAME_SIZE + 2] = {0};
                uint32_t cp                                    = gParamNameEdit.cursorPos;
                memcpy(editBuf, gParamNameEdit.buffer, cp);
                editBuf[cp]        = '|';
                memcpy(&editBuf[cp + 1], &gParamNameEdit.buffer[cp], strlen(gParamNameEdit.buffer) - cp + 1);
                gMorphLabelRect[i] = draw_button(mainArea, (tRectangle){{rectangle.coord.x - 5, rectangle.coord.y + 57}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, editBuf, (tRgb)RGB_WHITE);
            } else {
                gMorphLabelRect[i] = draw_button(mainArea, (tRectangle){{rectangle.coord.x - 5, rectangle.coord.y + 57}, {STANDARD_TEXT_HEIGHT * 4, textHeight}}, label, (tRgb)RGB_BACKGROUND_GREY);
            }
            register_click_region(gMorphLabelRect[i], eClickLayerPanel, morph_param_click_handler,
                                  morph_click_ctx(module, (uint32_t)(i + NUM_MORPHS)));

            rectangle.coord.x += (STANDARD_TEXT_HEIGHT * 4) + 5;
        }
    }
}

#ifdef __cplusplus
}
#endif

