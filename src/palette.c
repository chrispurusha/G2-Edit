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

// The drag-on module palette. Docs/module-palette-design.md carries the design and why it looks the
// way it does; the short version is that the instrument's own toolbar uses UNIFORM tiles rather than
// scaled-down module faces, and the module heights say why it has to — 94 of 170 modules are two
// rows, but Operator is twelve, and no single scale suits both.
//
// The band is an EXTENSION of the topbar rather than a mode that swaps its contents. A mode would
// hide the patch name, the voice count and the Patch Load meters exactly while modules are being
// added, and the manual ties adding modules to watching those meters. Growing the bar hides nothing
// and costs canvas only while the palette is open.

#include <string.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "globalVars.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "canvasCoords.h"
#include "menus.h"
#include "splitView.h"
#include "moduleGraphics.h"
#include "clickRegion.h"
#include "palette.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "selection.h"
#include "undo.h"
#include "graphics.h"

#define PALETTE_MAX_TILES     (32)         // the largest group is Osc at 19
#define PALETTE_GROUP_COLS    (8)
#define PALETTE_GROUP_ROWS    (2)
#define PALETTE_GROUP_W       (56.0)
#define PALETTE_GROUP_H       (16.0)
#define PALETTE_GROUP_X       (6.0)
// THE BAND'S PADDING IS SYMMETRIC, and these four have to be changed together to keep it so: the
// gap above the group grid, between the grid and the tiles, and below the tiles are all 5 px, which
// makes PALETTE_BAND_HEIGHT 5 + (2 rows x 17) + 5 + 26 + 5 = 74. Two rows of PALETTE_GROUP_H plus
// one pixel between them is the 34 in the middle.
#define PALETTE_GROUP_Y        (5.0)
#define PALETTE_TILE_W_MAX     (68.0)
#define PALETTE_TILE_W_MIN     (52.0)
#define PALETTE_ARROW_W        (14.0)
#define PALETTE_TILE_H         (26.0)
#define PALETTE_TILE_GAP       (4.0)
#define PALETTE_TILE_Y         (43.0)
#define PALETTE_TILE_X         (6.0)
#define PALETTE_DOT            (3.0)
#define PALETTE_DOT_GAP        (1.5)
#define PALETTE_MAX_DOTS       (5)
#define PALETTE_TILE_TEXT_H    (9.0)

static bool          gOpen;
static tPaletteGroup gGroup;
static double        gScroll;
static double        gTileW     = PALETTE_TILE_W_MAX; // shrunk to fit the group, floored at _MIN
static tRectangle    gArrowLeft;
static tRectangle    gArrowRight;
static bool          gScrollable;              // horizontal, for a group wider than the band

// Rebuilt every render so the hit test and the drawing can never disagree about where a tile is.
static tModuleType   gTile[PALETTE_MAX_TILES];
static tRectangle    gTileRect[PALETTE_MAX_TILES];
static uint32_t      gTileCount;
static tRectangle    gGroupRect[palGroupCount];
static int32_t       gHoverTile = -1;

static struct {
    bool        pressed;      // a tile is held but has not moved far enough to be a drag
    bool        active;       // it has, and a ghost is following the cursor
    tModuleType type;
    tCoord      pressCoord;
    tCoord      coord;
}                    gDrag;

bool palette_is_open(void) {
    return gOpen;
}

double palette_band_height(void) {
    return gOpen ? PALETTE_BAND_HEIGHT : 0.0;
}

tPaletteGroup palette_selected_group(void) {
    return gGroup;
}

void palette_select_group(tPaletteGroup group) {
    if (group < palGroupCount) {
        gGroup  = group;
        gScroll = 0.0;
        synthlib_request_redraw();
    }
}

// The canvas origin is derived from the theme's topBarHeight and nothing else, so opening the band
// is one call — see the note on configure_synthlib_theme() in graphics.c.
void palette_set_open(bool open) {
    if (gOpen == open) {
        return;
    }
    gOpen         = open;
    gDrag.active  = false;
    gDrag.pressed = false;
    apply_top_bar_height();
    synthlib_request_redraw();
}

void palette_toggle(void) {
    palette_set_open(!gOpen);
}

bool palette_drag_active(void) {
    return gDrag.active;
}

static double band_top(void) {
    return MENU_BAR_HEIGHT + TOP_BAR_HEIGHT;
}

// A module's connectors, split by direction and capped at what a tile can show. Colour comes from
// the same table the canvas uses, so a tile's dots and the module's own sockets always agree.
static uint32_t tile_connectors(tModuleType moduleType, tConnectorDir dir, tRgb * out, uint32_t max) {
    uint32_t count = 0;
    uint32_t i     = 0;

    for (i = 0; (i < array_size_connector_location_list()) && (count < max); i++) {
        if (  (connectorLocationList[i].moduleType == moduleType)
           && (connectorLocationList[i].direction == dir)) {
            out[count] = connectorColourMap[connectorLocationList[i].type];
            count++;
        }
    }

    return count;
}

static void draw_tile(tRectangle rect, tModuleType moduleType, bool hovered) {
    tRgb     body = (tRgb)MODULE_STANDARD_GREY;
    tRgb     dots[PALETTE_MAX_DOTS];
    uint32_t n    = 0;
    uint32_t i    = 0;

    set_rgb_colour(hovered ? (tRgb)RGB_GREY_7 : body);
    render_rectangle_with_border(mainArea, rect);

    // Inputs down the left edge, outputs down the right, coloured by connector type. This is the
    // information a scaled-down face could not carry at tile size: what the module takes and gives.
    n = tile_connectors(moduleType, connectorDirIn, dots, PALETTE_MAX_DOTS);

    for (i = 0; i < n; i++) {
        set_rgb_colour(dots[i]);
        render_rectangle(mainArea, (tRectangle){
            {rect.coord.x + 1.5,
             rect.coord.y + 2.0 + ((PALETTE_DOT + PALETTE_DOT_GAP) * (double)i)},
            {PALETTE_DOT, PALETTE_DOT}
        });
    }

    n = tile_connectors(moduleType, connectorDirOut, dots, PALETTE_MAX_DOTS);

    for (i = 0; i < n; i++) {
        set_rgb_colour(dots[i]);
        render_rectangle(mainArea, (tRectangle){
            {rect.coord.x + rect.size.w - 1.5 - PALETTE_DOT,
             rect.coord.y + 2.0 + ((PALETTE_DOT + PALETTE_DOT_GAP) * (double)i)},
            {PALETTE_DOT, PALETTE_DOT}
        });
    }

    set_rgb_colour((tRgb)RGB_BLACK);
    // Below the connector dots, and inset clear of both dot columns. TRUNCATED to what fits:
    // render_text does not clip to the rectangle it is given, so a name too long for a shrunk tile
    // would run straight through its neighbour.
    {
        char   label[CLAVIA_NAME_SIZE + 1] = {0};
        double room                        = rect.size.w - 16.0;
        size_t len                         = 0;

        COPY_STRING(label, gModuleProperties[moduleType].name);
        len = strlen(label);

        while ((len > 1) && (get_text_width(label, PALETTE_TILE_TEXT_H, eNoCache) > room)) {
            len--;
            label[len] = '\0';
        }
        render_text(mainArea, (tRectangle){
            {rect.coord.x + 8.0, rect.coord.y + 15.0},
            {room, PALETTE_TILE_TEXT_H}
        }, label);
    }
}

void palette_render(void) {
    double   top   = band_top();
    double   width = get_render_width() / gGlobalGuiScale;
    uint32_t g     = 0;
    uint32_t i     = 0;
    double   x     = 0.0;

    if (gOpen == false) {
        gTileCount = 0;
        return;
    }
    set_rgb_colour((tRgb)RGB_GREY_5);
    render_rectangle_with_border(mainArea, (tRectangle){{0.0, top}, {width, PALETTE_BAND_HEIGHT}});

    // Sixteen groups as a 2 x 8 grid, the way the instrument's own toolbar lays them out - a
    // compact block rather than a row, which leaves the whole width below for the tiles.
    for (g = 0; g < palGroupCount; g++) {
        uint32_t   col  = g % PALETTE_GROUP_COLS;
        uint32_t   row  = g / PALETTE_GROUP_COLS;
        tRectangle rect = {
            {
                PALETTE_GROUP_X + ((PALETTE_GROUP_W + 1.0) * (double)col),
                top + PALETTE_GROUP_Y + ((PALETTE_GROUP_H + 1.0) * (double)row)
            },
            {PALETTE_GROUP_W, PALETTE_GROUP_H}
        };

        gGroupRect[g] = rect;
        set_rgb_colour((g == gGroup) ? (tRgb)MODULE_BLUE_1 : (tRgb)RGB_BACKGROUND_GREY);
        render_rectangle_with_border(mainArea, rect);
        set_rgb_colour((g == gGroup) ? (tRgb)RGB_WHITE : (tRgb)RGB_BLACK);
        // render_text's coord.y is the TOP of the text, not its baseline - draw_button_split() is
        // the proof, offsetting its text rectangle by the button margin from the button's own top.
        // Both labels here were first written as though it were a baseline, which drew every group
        // name below its own button and through the row beneath it.
        render_text(mainArea, (tRectangle){
            {rect.coord.x + 4.0, rect.coord.y + 3.5},
            {PALETTE_GROUP_W - 8.0, PALETTE_TILE_TEXT_H}
        }, palette_group_name((tPaletteGroup)g));
    }

    gTileCount = palette_group_modules(gGroup, gTile, PALETTE_MAX_TILES);

    // TILES SHRINK TO FIT BEFORE THEY SCROLL. Osc is the largest group at 19, and nineteen tiles at
    // the full width run off the right of the default window - so the width is divided by the count
    // and only FLOORED, not fixed. Below that floor the names stop being readable, and at that point
    // the row scrolls instead, with arrows at the ends to say so.
    {
        double room  = width - (PALETTE_TILE_X * 2.0);
        double pitch = (gTileCount > 0) ? (room / (double)gTileCount) : PALETTE_TILE_W_MAX;
        double span  = 0.0;
        double max   = 0.0;

        gTileW      = pitch - PALETTE_TILE_GAP;

        if (gTileW > PALETTE_TILE_W_MAX) {
            gTileW = PALETTE_TILE_W_MAX;
        }

        if (gTileW < PALETTE_TILE_W_MIN) {
            gTileW = PALETTE_TILE_W_MIN;
        }
        span        = ((gTileW + PALETTE_TILE_GAP) * (double)gTileCount) - PALETTE_TILE_GAP;
        gScrollable = (span > room);

        // Clamped here rather than at the wheel event, so a window resize or a group change cannot
        // leave the row parked past its own end.
        max         = gScrollable ? (span - room) : 0.0;

        if (gScroll > max) {
            gScroll = max;
        }

        if (gScroll < 0.0) {
            gScroll = 0.0;
        }
    }
    x           = PALETTE_TILE_X - gScroll;

    for (i = 0; i < gTileCount; i++) {
        gTileRect[i] = (tRectangle){
            {
                x, top + PALETTE_TILE_Y
            }, {
                gTileW, PALETTE_TILE_H
            }
        };

        if ((gTileRect[i].coord.x + gTileW > 0.0) && (gTileRect[i].coord.x < width)) {
            draw_tile(gTileRect[i], gTile[i], (gHoverTile == (int32_t)i));
        }
        x           += gTileW + PALETTE_TILE_GAP;
    }

    // The arrows exist only while the row is longer than the window, and they ARE the affordance for
    // it: the wheel on its own leaves the overflow invisible, and so unreachable by anyone who does
    // not already know it is there.
    gArrowLeft  = (tRectangle){
        {
            0.0, 0.0
        }, {
            0.0, 0.0
        }
    };
    gArrowRight = gArrowLeft;

    if (gScrollable) {
        gArrowLeft  = (tRectangle){
            {
                0.0, top + PALETTE_TILE_Y
            }, {
                PALETTE_ARROW_W, PALETTE_TILE_H
            }
        };
        gArrowRight = (tRectangle){
            {
                width - PALETTE_ARROW_W, top + PALETTE_TILE_Y
            }, {
                PALETTE_ARROW_W, PALETTE_TILE_H
            }
        };

        set_rgb_colour((tRgb)RGB_BACKGROUND_GREY);
        render_rectangle_with_border(mainArea, gArrowLeft);
        render_rectangle_with_border(mainArea, gArrowRight);
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){
            {
                gArrowLeft.coord.x + 4.0, gArrowLeft.coord.y + 8.0
            }, {
                PALETTE_ARROW_W, PALETTE_TILE_TEXT_H
            }
        }, "<");
        render_text(mainArea, (tRectangle){
            {
                gArrowRight.coord.x + 4.0, gArrowRight.coord.y + 8.0
            }, {
                PALETTE_ARROW_W, PALETTE_TILE_TEXT_H
            }
        }, ">");
    }

    // THE GHOST IS A REAL MODULE FACE - dials, buttons, connectors and all - not an outline and not
    // the tile. It is built by module_prototype(), the same defaults create_module_at() lays down,
    // so what follows the cursor is exactly what the drop will produce, drawn at the canvas's own
    // zoom in the column and row it would occupy.
    //
    // CLICK REGISTRATION IS SUPPRESSED WHILE IT DRAWS. render_module() and every widget under it
    // register click regions keyed by the module's slot/location/index, and a ghost has none - it
    // would be registering regions for a module that does not exist, on top of whichever real one
    // owns that index. An EMPTY click clip is the seam for that: register_click_region() drops
    // anything that falls outside the clip, so nothing drawn here can be clicked.
    if (gDrag.active) {
        int32_t                 pane   = split_view_pane_at(gDrag.coord);
        bool                    onPane = (pane >= 0);
        tModule                 ghost  = {0};
        uint32_t                column = 0;
        uint32_t                row    = 0;

        static const tRectangle none   = {
            {
                0.0, 0.0
            },{
                0.0, 0.0
            }
        };

        // OVER THE BAND ITSELF - which is where every drag STARTS - there is no pane under the
        // cursor, and a plain outline was drawn there instead. That left the first moments of every
        // drag showing a rectangle with none of the module's controls in it (CT, 2026-09-07: "the
        // components aren't in the module representation"). So the ghost goes straight into the
        // FOCUSED pane at the top of the cursor's own column and is a full face from the outset;
        // once the cursor enters a pane it tracks the drop position exactly.
        if (!onPane) {
            pane = (int32_t)split_view_focused_pane();
        }
        set_module_pane((uint32_t)pane);
        gLocation          = (tLocation)split_view_location_for_pane((uint32_t)pane);

        if (onPane) {
            (void)split_view_focus_at(gDrag.coord);
            convert_mouse_coord_to_module_column_row(&column, &row, gDrag.coord);
        } else {
            convert_mouse_coord_to_module_column_row(&column, NULL, gDrag.coord);
            row = 0;
        }
        module_prototype(gDrag.type, &ghost);
        ghost.key.slot     = (uint32_t)gSlot;
        ghost.key.location = gLocation;
        ghost.key.index    = 0;
        ghost.column       = column;
        ghost.row          = row;

        // CLICK REGISTRATION IS SUPPRESSED WHILE IT DRAWS. render_module() and every widget under
        // it register regions keyed by the module's slot/location/index, and a ghost has none - it
        // would be registering regions for a module that does not exist, on top of whichever real
        // one owns that index. An EMPTY click clip is the seam: register_click_region() drops
        // anything falling outside the clip, so nothing drawn here can be clicked.
        module_pane_clip_begin();
        set_click_region_clip(&none);
        render_module(&ghost);
        module_pane_clip_end();
    }
}

static int32_t tile_at(tCoord coord) {
    uint32_t i = 0;

    for (i = 0; i < gTileCount; i++) {
        if (within_rectangle(coord, gTileRect[i])) {
            return (int32_t)i;
        }
    }

    return -1;
}

bool palette_add_module(tModuleType moduleType) {
    uint32_t       column         = 0;
    uint32_t       row            = 0;
    tUndoMoveEntry displaced[MAX_NUM_MODULES];
    uint32_t       displacedCount = 0;
    int32_t        created        = 0;

    // Under the focused module if there is one, otherwise the top of column 0 - which is what the
    // manual describes for its double-click ("below the currently focused module").
    tModule *      focus          = get_module(gSelection.count > 0 ? gSelection.keys[0]
                                               : (tModuleKey){gSlot, gLocation, 0});

    if (focus != NULL) {
        column = focus->column;
        row    = focus->row + gModuleProperties[focus->type].height;
    }
    displacedCount = module_positions_snapshot((uint32_t)gSlot, (uint32_t)gLocation, displaced);
    created        = create_module_at(moduleType, column, row, true);

    if (created < 0) {
        return false;
    }
    undo_push_create_module((tModuleKey){gSlot, gLocation, (uint32_t)created},
                            displaced, module_positions_changed(displaced, displacedCount));
    synthlib_request_redraw();
    return true;
}

void palette_begin_drag(tModuleType type, tCoord coord) {
    gDrag.pressed    = true;
    gDrag.active     = true;
    gDrag.type       = type;
    gDrag.pressCoord = coord;
    gDrag.coord      = coord;
    synthlib_request_redraw();
}

bool palette_left_down(tCoord coord) {
    uint32_t g = 0;
    int32_t  t = 0;

    if (gOpen == false) {
        return false;
    }

    for (g = 0; g < palGroupCount; g++) {
        if (within_rectangle(coord, gGroupRect[g])) {
            palette_select_group((tPaletteGroup)g);
            return true;
        }
    }

    if (gScrollable && within_rectangle(coord, gArrowLeft)) {
        gScroll -= (gTileW + PALETTE_TILE_GAP) * 3.0;
        synthlib_request_redraw();
        return true;
    }

    if (gScrollable && within_rectangle(coord, gArrowRight)) {
        gScroll += (gTileW + PALETTE_TILE_GAP) * 3.0;
        synthlib_request_redraw();
        return true;
    }
    t = tile_at(coord);

    if (t >= 0) {
        // THE GHOST IS UP FROM THE MOMENT THE BUTTON GOES DOWN, with no movement threshold first.
        // A slop distance was tried and taken out: it exists to stop a click being mistaken for a
        // drag, but there is nothing else a press on a tile can mean here, and it left the first
        // few pixels of every drag showing nothing at all. A press that never reaches a module area
        // is simply abandoned on release, which costs the user one frame of ghost and no more.
        gDrag.pressed    = true;
        gDrag.active     = true;
        gDrag.type       = gTile[t];
        gDrag.pressCoord = coord;
        gDrag.coord      = coord;
        return true;
    }
    // Anywhere else in the band is still ours - swallow it so a click on the band's background does
    // not fall through to the canvas underneath and start a rubber-band selection.
    return (coord.y >= band_top()) && (coord.y < (band_top() + PALETTE_BAND_HEIGHT));
}

void palette_cursor_moved(tCoord coord) {
    int32_t hover = -1;

    if (gOpen == false) {
        return;
    }

    if (gDrag.pressed) {
        gDrag.coord = coord;
        synthlib_request_redraw();
        return;
    }
    hover = tile_at(coord);

    if (hover != gHoverTile) {
        gHoverTile = hover;
        synthlib_request_redraw();
    }
}

bool palette_left_up(tCoord coord) {
    bool wasDragging = gDrag.active;
    bool wasPressed  = gDrag.pressed;

    if ((gOpen == false) || (wasPressed == false)) {
        return false;
    }
    gDrag.pressed = false;
    gDrag.active  = false;
    synthlib_request_redraw();

    (void)wasDragging;   // always true now that the ghost is up from the press; kept for the read

    // ABANDONED unless the release is over a module area. Released on the band, the topbar, the
    // menu bar or the split bar, nothing is created and nothing is disturbed - which is also what a
    // plain click on a tile amounts to, since the press now starts a drag immediately.
    if (coord.y < (band_top() + PALETTE_BAND_HEIGHT)) {
        return true;
    }
    {
        uint32_t       column         = 0;
        uint32_t       row            = 0;
        tUndoMoveEntry displaced[MAX_NUM_MODULES];
        uint32_t       displacedCount = 0;
        int32_t        created        = 0;

        // THE DROP LANDS IN THE PANE UNDER THE CURSOR, not the focused one. Everything downstream -
        // module_area() inside convert_mouse_coord_to_module_column_row(), the scroll offsets it
        // adds, and create_module_at()'s own use of gLocation - reads the FOCUSED pane, so focusing
        // the pane being dropped on is what makes all three agree. Without it every drop landed in
        // whichever half had focus: dragging onto the Voice Area created the module in the FX Area.
        // WHETHER THE COORDINATE IS IN A PANE IS split_view_pane_at()'S ANSWER, NOT
        // split_view_focus_at()'S. focus_at() returns whether the focus MOVED, so dropping into the
        // pane that already had focus returned false - and this treated that as "outside a pane"
        // and threw the drop away. Every drop into the already-focused half silently did nothing,
        // which is most of them (CT, 2026-09-07: "Dropping a new module isn't working").
        if (split_view_pane_at(coord) < 0) {
            return true;               // the split bar, the scrollbars, or outside both panes
        }
        (void)split_view_focus_at(coord);
        convert_mouse_coord_to_module_column_row(&column, &row, coord);
        displacedCount = module_positions_snapshot((uint32_t)gSlot, (uint32_t)gLocation, displaced);
        created        = create_module_at(gDrag.type, column, row, true);

        if (created >= 0) {
            undo_push_create_module((tModuleKey){gSlot, gLocation, (uint32_t)created},
                                    displaced, module_positions_changed(displaced, displacedCount));
        }
    }
    return true;
}

bool palette_scroll(double delta, tCoord coord) {
    if (  (gOpen == false) || (coord.y < band_top())
       || (coord.y >= (band_top() + PALETTE_BAND_HEIGHT))) {
        return false;
    }
    gScroll -= delta * (gTileW + PALETTE_TILE_GAP);
    synthlib_request_redraw();
    return true;
}
