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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include <math.h>

#include "splitView.h"
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibGlobals.h"
#include "globalVars.h"
#include "graphics.h"
#include "utilsGraphics.h"

tSplitView gSplitView = {0};

// Height of the bar itself, in the same window units module_area() works in. The two panes give up
// half of it each, so the divider fraction stays the visual centre of the bar.
#define SV_BAR_H              (14.0)
#define SV_BUTTON_W           (18.0)
#define SV_BUTTON_GAP         (3.0)
#define SV_BAR_GRAB_MARGIN    (5.0)   // extra grab height above and below the drawn bar

void split_view_init(void) {
    gSplitView.focusedPane     = 0;
    gSplitView.restorePosition = 0;
    gSplitView.dragging        = false;
    gSplitView.dirty           = false;
}

// ─── Pane geometry ───────────────────────────────────────────────────────────

// The whole canvas band, i.e. what the two panes and the bar divide up between them.
static double band_height(void) {
    set_module_pane_extent(0, 0.0, 1.0);
    return module_area_for_pane(0).size.h;
}

// The Voice Area pane's height in pixels, which is what barPosition stores. Clamped here rather
// than at the source so a patch carrying the reference's "bigger than any window" default (4000)
// simply means "Voice Area takes everything", exactly as it does in the original.
static double top_pane_height(double band) {
    double usable = band - SPLIT_BAR_FOOTPRINT;
    double top    = (double)gPatchDescr[gSlot].barPosition;

    if (usable < 0.0) {
        usable = 0.0;
    }

    if (top > usable) {
        top = usable;
    }

    if (top < 0.0) {
        top = 0.0;
    }

    // Snap the ends: a pane is either genuinely collapsed or big enough to use. Without this a
    // drag can leave a two-pixel sliver that shows nothing and cannot be clicked in.
    if (top < SPLIT_MIN_PANE) {
        top = 0.0;
    }

    if ((usable - top) < SPLIT_MIN_PANE) {
        top = usable;
    }
    return top;
}

void split_view_apply(void) {
    double band       = band_height();
    double top        = top_pane_height(band);

    if (band <= 0.0) {
        set_module_pane_count(1);
        set_module_pane_extent(0, 0.0, 1.0);
        return;
    }
    // The TOP pane gives up a strip at its foot for its own horizontal scrollbar, PLUS the same gap
    // the divider gets, so modules never butt straight up against the bar. The bottom pane doesn't
    // need to reserve anything: the canvas band already stops SCROLLBAR_WIDTH + MODULE_MARGIN above
    // the window bottom, and that reserved strip is exactly where its bar lands once the same gap is
    // applied below its own foot.
    double topModules = top - SCROLLBAR_WIDTH - SPLIT_BAR_GAP;

    if (topModules < 0.0) {
        topModules = 0.0;
    }
    set_module_pane_count(2);
    set_module_pane_extent(0, 0.0, topModules / band);
    set_module_pane_extent(1, (top + SPLIT_BAR_FOOTPRINT) / band, (band - top - SPLIT_BAR_FOOTPRINT) / band);

    // Focus can't sit on a collapsed pane — every gLocation reader would then be pointed at an area
    // that isn't on screen.
    if ((top <= 0.0) && (gSplitView.focusedPane == 0)) {
        gSplitView.focusedPane = 1;
        gLocation              = (tLocation)split_view_location_for_pane(1);
    } else if (((band - top - SPLIT_BAR_FOOTPRINT) <= 0.0) && (gSplitView.focusedPane == 1)) {
        gSplitView.focusedPane = 0;
        gLocation              = (tLocation)split_view_location_for_pane(0);
    }
}

uint32_t split_view_location_for_pane(uint32_t pane) {
    // Voice Area on top, FX below — the order the original uses, and the order the signal flows in
    // a patch, so the FX area reads as "after" the voice area rather than above it.
    return (pane == 0) ? (uint32_t)locationVa : (uint32_t)locationFx;
}

double split_view_pane_height(uint32_t location) {
    return module_area_for_pane((location == (uint32_t)locationFx) ? 1 : 0).size.h;
}

bool split_view_is_full(uint32_t location) {
    return split_view_pane_height((location == (uint32_t)locationFx) ? (uint32_t)locationVa : (uint32_t)locationFx) <= 0.0;
}

uint32_t split_view_focused_pane(void) {
    return (gSplitView.focusedPane < module_pane_count()) ? gSplitView.focusedPane : 0;
}

int32_t split_view_pane_at(tCoord coord) {
    for (uint32_t p = 0; p < module_pane_count(); p++) {
        tRectangle r = module_area_for_pane(p);

        if ((r.size.h > 0.0) && within_rectangle(coord, r)) {
            return (int32_t)p;
        }
    }

    return -1;
}

bool split_view_focus_at(tCoord coord) {
    int32_t pane  = split_view_pane_at(coord);

    if (pane < 0) {
        return false;
    }
    bool    moved = ((uint32_t)pane != gSplitView.focusedPane);

    gSplitView.focusedPane = (uint32_t)pane;

    // gLocation is what the other 40-odd readers in the app use to mean "the Location I am
    // editing". Pointing it at the focused pane's Location is what keeps all of them correct
    // without any of them learning that panes exist.
    gLocation              = (tLocation)split_view_location_for_pane((uint32_t)pane);
    set_module_pane((uint32_t)pane);
    return moved;
}

// Records a new divider position. Remembers the last non-collapsed one so the double-arrow has
// somewhere to go back to, and marks the patch descriptor for sending — once, on mouse-up, rather
// than on every frame of a drag.
static void set_bar_position(double pixels) {
    double band   = band_height();
    double usable = band - SPLIT_BAR_FOOTPRINT;

    if (pixels < 0.0) {
        pixels = 0.0;
    }

    if (pixels > SPLIT_POS_MAX) {
        pixels = SPLIT_POS_MAX;
    }

    if ((pixels > 0.0) && (pixels < usable)) {
        gSplitView.restorePosition = (uint16_t)pixels;
    }
    gPatchDescr[gSlot].barPosition = (uint16_t)pixels;
    gSplitView.dirty               = true;
    split_view_apply();
    synthlib_request_redraw();
}

// Back to the last position that had both areas visible. With nothing remembered — a patch that
// arrived collapsed — an even split is the sensible landing place.
void split_view_restore_balance(void) {
    double band = band_height();
    double back = (gSplitView.restorePosition > 0)
                  ? (double)gSplitView.restorePosition
                  : ((band - SPLIT_BAR_FOOTPRINT) / 2.0);

    set_bar_position(back);
}

void split_view_show_full(uint32_t location) {
    if (location == (uint32_t)locationFx) {
        set_bar_position(0.0);           // Voice Area collapsed, FX gets everything
    } else {
        set_bar_position(SPLIT_POS_MAX); // clamps to the whole band, as the reference's 4000 does
    }
    gLocation = (tLocation)location;
}

// ─── Rendering ───────────────────────────────────────────────────────────────

// The bar is only SV_BAR_H tall, which is a small target and gives no cursor feedback — miss it and
// the click lands in a pane and starts a rubber-band select instead, which reads as "the drag does
// not work". So the GRAB area is taller than the drawn bar, the usual treatment for a thin
// splitter. The buttons are hit-tested first and keep their own exact rects.
static tRectangle split_bar_grab_rect(void) {
    tRectangle r = gSplitView.barRect;

    if (r.size.h <= 0.0) {
        return r;
    }
    r.coord.y -= SV_BAR_GRAB_MARGIN;
    r.size.h  += SV_BAR_GRAB_MARGIN * 2.0;
    return r;
}

void render_split_bar(void) {
    // Always drawn — with the divider at an end, the bar is the only handle for bringing the
    // collapsed area back, so it can never be the thing that disappears.
    tRectangle top    = module_area_for_pane(0);
    tRectangle bottom = module_area_for_pane(1);

    // Anchored to the BOTTOM pane and given a fixed height, rather than filling whatever space lies
    // between the two panes. That gap is no longer just the bar: the top pane also gives up a strip
    // at its foot for its own horizontal scrollbar, so measuring the gap made the bar 26px tall and
    // drew it straight over that scrollbar. Working back from the bottom pane's top edge keeps the
    // order — top pane, its scrollbar, gap, bar, gap, bottom pane — and stays correct when the top
    // pane is collapsed and has no scrollbar at all.
    double     y      = bottom.coord.y - SPLIT_BAR_GAP - SPLIT_BAR_HEIGHT;
    double     h      = SPLIT_BAR_HEIGHT;

    gSplitView.barRect = (tRectangle){{
                                          top.coord.x, y
                                      }, {
                                          top.size.w, h
                                      }
    };

    set_rgb_colour((tRgb)RGB_GREY_5);
    render_rectangle(mainArea, gSplitView.barRect);

    // A pair of grip lines, so the bar reads as draggable rather than as a border.
    set_rgb_colour((tRgb)RGB_GREY_3);
    double     cx     = top.coord.x + (top.size.w / 2.0);

    for (int i = -1; i <= 1; i += 2) {
        render_line(mainArea, (tCoord){cx - 18.0, y + (h / 2.0) + (i * 2.0)},
                    (tCoord){cx + 18.0, y + (h / 2.0) + (i * 2.0)}, 1.0);
    }

    // The three buttons the manual describes, at the right-hand end: give the top pane everything,
    // give the bottom pane everything, and come back to the remembered split. Drawn on the bar
    // itself so they travel with it.
    double     bx     = top.coord.x + top.size.w - ((SV_BUTTON_W + SV_BUTTON_GAP) * 3.0) - 4.0;
    double     midY   = y + (h / 2.0);

    gSplitView.upButton      = (tRectangle){{
                                                bx, y + 1.0
                                            }, {
                                                SV_BUTTON_W, h - 2.0
                                            }
    };
    bx                      += SV_BUTTON_W + SV_BUTTON_GAP;
    gSplitView.downButton    = (tRectangle){{
                                                bx, y + 1.0
                                            }, {
                                                SV_BUTTON_W, h - 2.0
                                            }
    };
    bx                      += SV_BUTTON_W + SV_BUTTON_GAP;
    gSplitView.restoreButton = (tRectangle){{
                                                bx, y + 1.0
                                            }, {
                                                SV_BUTTON_W, h - 2.0
                                            }
    };

    set_rgb_colour((tRgb)RGB_GREY_9);
    render_rectangle(mainArea, gSplitView.upButton);
    render_rectangle(mainArea, gSplitView.downButton);
    render_rectangle(mainArea, gSplitView.restoreButton);

    // NOTE tTriangle's second and third vertices are RELATIVE to the first, not absolute — passing
    // three absolute points draws a garbled shape, which is exactly what the first attempt did.
    set_rgb_colour((tRgb)RGB_BLACK);
    {
        double half = SV_BUTTON_W / 2.0;
        double u    = gSplitView.upButton.coord.x + half;
        double d    = gSplitView.downButton.coord.x + half;
        double r    = gSplitView.restoreButton.coord.x + half;

        // Up arrow: the top pane grows to fill the window.
        render_triangle(mainArea, (tTriangle){{u, midY - 4.0}, {-5.0, 7.0}, {5.0, 7.0}});
        // Down arrow: the bottom pane grows to fill the window.
        render_triangle(mainArea, (tTriangle){{d, midY + 4.0}, {-5.0, -7.0}, {5.0, -7.0}});
        // Restore: the two arrows back to back, pointing apart.
        render_triangle(mainArea, (tTriangle){{r, midY - 5.0}, {-5.0, 4.0}, {5.0, 4.0}});
        render_triangle(mainArea, (tTriangle){{r, midY + 5.0}, {-5.0, -4.0}, {5.0, -4.0}});
    }
}

// ─── Mouse ───────────────────────────────────────────────────────────────────

// A divider move is a patch edit — barPosition lives in the patch descriptor — so it goes to the
// G2 like any other. Sent on mouse-up only: a drag would otherwise fire a descriptor write per
// frame, and this is exactly the kind of burst that loses commands to the patch-version race.
void split_view_flush_position(void) {
    tMessageContent msg = {0};

    if (!gSplitView.dirty) {
        return;
    }
    gSplitView.dirty = false;
    msg.cmd          = eMsgCmdWritePatchDescr;
    msg.slot         = gSlot;
    msg_send(&gToUsbThread, &msg);
}

bool handle_split_bar_mouse(tCoord coord, tMouseButton mouseButton) {
    if (mouseButton == mouseButtonLeftDown) {
        if (  within_rectangle(coord, gSplitView.upButton)
           || within_rectangle(coord, gSplitView.downButton)
           || within_rectangle(coord, gSplitView.restoreButton)) {
            return true;   // consumed; acted on at mouse-up, as every other button here is
        }

        if (within_rectangle(coord, split_bar_grab_rect())) {
            gSplitView.dragging = true;
            return true;
        }
        return false;
    }

    if (mouseButton == mouseButtonLeftUp) {
        bool wasDragging = gSplitView.dragging;

        gSplitView.dragging = false;

        if (within_rectangle(coord, gSplitView.upButton)) {
            split_view_show_full((uint32_t)locationVa);
            split_view_flush_position();
            return true;
        }

        if (within_rectangle(coord, gSplitView.downButton)) {
            split_view_show_full((uint32_t)locationFx);
            split_view_flush_position();
            return true;
        }

        if (within_rectangle(coord, gSplitView.restoreButton)) {
            split_view_restore_balance();
            split_view_flush_position();
            return true;
        }

        if (wasDragging) {
            split_view_flush_position();
            return true;
        }

        if (within_rectangle(coord, gSplitView.barRect)) {
            return true;
        }
    }
    return false;
}

void handle_split_bar_cursor_pos(tCoord coord) {
    if (!gSplitView.dragging) {
        return;
    }
    tRectangle whole = module_area_for_pane(0);   // pane 0 starts at the band's top edge, always

    set_bar_position(coord.y - whole.coord.y);
}


// ─── Pane scrollbars ─────────────────────────────────────────────────────────
//
// One vertical bar per visible pane, plus a horizontal bar for the focused pane. The window's old
// single shared pair could not do the vertical: two panes scroll independently, so one thumb could
// only ever tell the truth about one of them.
//
// THE THUMB IS PROPORTIONAL, which is the thing that makes a scrollbar read as a scrollbar on any
// desktop: its length is the fraction of the content you can currently see, so it grows as you zoom
// out and shrinks as you zoom in, and its position is that fraction slid along the remaining track.
// The old bars drew a fixed-length block that said nothing about how much content there was.
//
// The thumb is also DERIVED from the pane's scroll percent every frame rather than tracked
// alongside it, so it cannot drift out of step with a pane scrolled by the wheel or by a zoom.

#define PANE_SCROLL_MIN_THUMB    (24.0)   // never so short it can't be grabbed
#define PANE_SCROLL_INSET        (3.0)    // thumb floats inside its track, rather than filling it

static tRectangle sVTrack[MAX_MODULE_PANES];
static tRectangle sVThumb[MAX_MODULE_PANES];
static tRectangle sHTrack[MAX_MODULE_PANES];
static tRectangle sHThumb[MAX_MODULE_PANES];
static int32_t    sVDragPane        = -1;   // pane whose VERTICAL bar is being dragged, -1 for none
static int32_t    sHDragPane        = -1;   // ditto horizontal
static double     sScrollGrabOffset = 0.0;

// Total canvas extent at the current zoom — the same numbers calc_scroll_x/y() divide by, which is
// what makes the thumb's proportion agree with how far the pane can actually travel.
static double content_height(void) {
    return get_zoom_factor() * (double)((MAX_ROWS + 1) + (MAX_ROWS_MODULE - 1)) * MODULE_Y_SPAN;
}

static double content_width(void) {
    return get_zoom_factor() * (double)(MAX_COLUMNS + 1) * MODULE_X_SPAN;
}

// Thumb length for a track showing `visible` of `content`, and the travel left over for it.
static double thumb_length(double track, double visible, double content) {
    double len = (content > 0.0) ? (track * (visible / content)) : track;

    if (len > track) {
        len = track;
    }

    if (len < PANE_SCROLL_MIN_THUMB) {
        len = PANE_SCROLL_MIN_THUMB;
    }
    return len;
}

// Draws the thumb with ROUNDED ENDS — a body rectangle short by one radius at each end, capped
// with a filled circle. Square ends made the thumb look truncated, as though it had been clipped by
// the track rather than sitting in it, which is the one detail that stops a scrollbar reading as a
// scrollbar. The radius is half the thumb's short side, so the caps are exact semicircles whatever
// the bar's width; PANE_SCROLL_MIN_THUMB is comfortably more than one diameter, so the body never
// inverts.
static void draw_thumb(tRectangle thumb, bool vertical) {
    double radius = (vertical ? thumb.size.w : thumb.size.h) / 2.0;

    if (radius <= 0.0) {
        return;
    }

    if (vertical) {
        render_rectangle(mainArea, (tRectangle){{
                                                    thumb.coord.x, thumb.coord.y + radius
                                                }, {
                                                    thumb.size.w, thumb.size.h - (radius * 2.0)
                                                }
                         });
        render_circle_part(mainArea, (tCoord){thumb.coord.x + radius, thumb.coord.y + radius}, radius, 12, 0, 12);
        render_circle_part(mainArea, (tCoord){thumb.coord.x + radius, thumb.coord.y + thumb.size.h - radius}, radius, 12, 0, 12);
        return;
    }
    render_rectangle(mainArea, (tRectangle){{
                                                thumb.coord.x + radius, thumb.coord.y
                                            }, {
                                                thumb.size.w - (radius * 2.0), thumb.size.h
                                            }
                     });
    render_circle_part(mainArea, (tCoord){thumb.coord.x + radius, thumb.coord.y + radius}, radius, 12, 0, 12);
    render_circle_part(mainArea, (tCoord){thumb.coord.x + thumb.size.w - radius, thumb.coord.y + radius}, radius, 12, 0, 12);
}

static void draw_track_and_thumb(tRectangle track, tRectangle thumb, bool vertical) {
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_rectangle(mainArea, track);
    set_rgb_colour((tRgb)RGB_GREY_3);
    draw_thumb(thumb, vertical);
}

void render_pane_scrollbars(void) {
    double renderWidth  = get_render_width() / gGlobalGuiScale;
    double renderHeight = get_render_height() / gGlobalGuiScale;
    double x            = renderWidth - SCROLLBAR_WIDTH;

    for (uint32_t pane = 0; pane < MAX_MODULE_PANES; pane++) {
        tRectangle r       = (pane < module_pane_count()) ? module_area_for_pane(pane) : (tRectangle){
            0
        };

        if (r.size.h <= 0.0) {
            sVTrack[pane] = (tRectangle){
                0
            };
            sVThumb[pane] = (tRectangle){
                0
            };
            continue;
        }
        sVTrack[pane] = (tRectangle){{
                                         x, r.coord.y
                                     }, {
                                         SCROLLBAR_WIDTH, r.size.h
                                     }
        };

        uint32_t   prev    = module_pane();

        set_module_pane(pane);
        double     percent = get_y_scroll_percent();

        set_module_pane(prev);

        double     len     = thumb_length(r.size.h, r.size.h, content_height());
        double     travel  = r.size.h - len;

        sVThumb[pane] = (tRectangle){{
                                         x + PANE_SCROLL_INSET, r.coord.y + ((percent / 100.0) * travel)
                                     }, {
                                         SCROLLBAR_WIDTH - (PANE_SCROLL_INSET * 2.0), len
                                     }
        };
        draw_track_and_thumb(sVTrack[pane], sVThumb[pane], true);
    }

    // One horizontal bar per pane too. A single shared bar could only ever be right about one of
    // them — the wheel already scrolls each pane's X independently, so a shared bar would either
    // misreport a pane or drag them into lockstep and undo what the wheel just did. The reference
    // does the same: its layout adds a scrollbar's height into the TOP pane's extent.
    for (uint32_t pane = 0; pane < MAX_MODULE_PANES; pane++) {
        tRectangle r       = (pane < module_pane_count()) ? module_area_for_pane(pane) : (tRectangle){
            0
        };

        if (r.size.h <= 0.0) {
            sHTrack[pane] = (tRectangle){
                0
            };
            sHThumb[pane] = (tRectangle){
                0
            };
            continue;
        }
        // Both bars stop the same distance clear of the bottom-right corner as the vertical ones do
        // of the window bottom, so the two meet symmetrically rather than one running further in.
        double     right   = renderWidth - SCROLLBAR_WIDTH - MODULE_MARGIN;

        // One gap below the modules, matching the divider's — and for the bottom pane this puts the
        // bar precisely in the strip the canvas band already leaves at the window bottom.
        sHTrack[pane] = (tRectangle){{
                                         SCROLLBAR_MARGIN, r.coord.y + r.size.h + SPLIT_BAR_GAP
                                     }, {
                                         right - SCROLLBAR_MARGIN, SCROLLBAR_WIDTH
                                     }
        };

        uint32_t   prev    = module_pane();

        set_module_pane(pane);
        double     percent = get_x_scroll_percent();

        set_module_pane(prev);

        double     len     = thumb_length(sHTrack[pane].size.w, r.size.w, content_width());
        double     travel  = sHTrack[pane].size.w - len;

        sHThumb[pane] = (tRectangle){{
                                         sHTrack[pane].coord.x + ((percent / 100.0) * travel),
                                         sHTrack[pane].coord.y + PANE_SCROLL_INSET
                                     }, {
                                         len, SCROLLBAR_WIDTH - (PANE_SCROLL_INSET * 2.0)
                                     }
        };
        draw_track_and_thumb(sHTrack[pane], sHThumb[pane], false);
    }
}

bool handle_pane_scrollbar_click(tCoord coord) {
    // Thumbs first, so grabbing one never gets mistaken for a click on the track behind it.
    for (uint32_t pane = 0; pane < MAX_MODULE_PANES; pane++) {
        if ((sVThumb[pane].size.h > 0.0) && within_rectangle(coord, sVThumb[pane])) {
            sVDragPane        = (int32_t)pane;
            sScrollGrabOffset = coord.y - sVThumb[pane].coord.y;
            return true;
        }

        if ((sHThumb[pane].size.w > 0.0) && within_rectangle(coord, sHThumb[pane])) {
            sHDragPane        = (int32_t)pane;
            sScrollGrabOffset = coord.x - sHThumb[pane].coord.x;
            return true;
        }
    }

    // A click on a track but off its thumb jumps to roughly where you clicked, as every other
    // scrollbar does — grabbing the thumb's centre so the jump lands under the cursor.
    for (uint32_t pane = 0; pane < MAX_MODULE_PANES; pane++) {
        if ((sVTrack[pane].size.h > 0.0) && within_rectangle(coord, sVTrack[pane])) {
            sVDragPane        = (int32_t)pane;
            sScrollGrabOffset = sVThumb[pane].size.h / 2.0;
            handle_pane_scrollbar_drag(coord);
            return true;
        }

        if ((sHTrack[pane].size.w > 0.0) && within_rectangle(coord, sHTrack[pane])) {
            sHDragPane        = (int32_t)pane;
            sScrollGrabOffset = sHThumb[pane].size.w / 2.0;
            handle_pane_scrollbar_drag(coord);
            return true;
        }
    }

    return false;
}

bool pane_scrollbar_dragging(void) {
    return (sVDragPane >= 0) || (sHDragPane >= 0);
}

void pane_scrollbar_release(void) {
    sVDragPane = -1;
    sHDragPane = -1;
}

static double percent_from(double pos, double origin, double travel) {
    double percent = (travel > 0.0) ? (((pos - origin) / travel) * 100.0) : 0.0;

    if (percent < 0.0) {
        percent = 0.0;
    }

    if (percent > 100.0) {
        percent = 100.0;
    }
    return percent;
}

void handle_pane_scrollbar_drag(tCoord coord) {
    uint32_t prev = module_pane();

    if (sVDragPane >= 0) {
        tRectangle r = module_area_for_pane((uint32_t)sVDragPane);

        set_module_pane((uint32_t)sVDragPane);
        set_y_scroll_percent(percent_from(coord.y - sScrollGrabOffset, r.coord.y,
                                          r.size.h - sVThumb[sVDragPane].size.h));
        set_module_pane(prev);
        synthlib_request_redraw();
        return;
    }

    if (sHDragPane >= 0) {
        set_module_pane((uint32_t)sHDragPane);
        set_x_scroll_percent(percent_from(coord.x - sScrollGrabOffset, sHTrack[sHDragPane].coord.x,
                                          sHTrack[sHDragPane].size.w - sHThumb[sHDragPane].size.w));
        set_module_pane(prev);
        synthlib_request_redraw();
    }
}

#ifdef __cplusplus
}
#endif
