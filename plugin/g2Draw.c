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
// Notes: Docs/code-notes/g2Draw.c.md - "// notes §k" refers there.

// notes §1

#define GL_SILENCE_DEPRECATION    1

#include <OpenGL/gl.h>

#include "sysIncludes.h"
// notes §2
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibTypes.h"
#include "geometry.h"
#include "utilsGraphics.h"
#include "clickRegion.h"
#include "types.h"
#include "globalVars.h"
#include "moduleGraphics.h"
#include "soundEngine.h"
#include "synthlibHost.h"
#include "canvasCoords.h"
#include "mouseHandle.h"

#include "contextMenu.h"
#include "menuBar.h"
#include "g2Menu.h"
#include "prefs.h"
#include "g2Prefs.h"
#include "topbarRender.h"
#include "dataBase.h"   // set_patch_name_from_filename(), init_patch()
#include "topbarResourcesAccess.h"
#include "mouseTopbar.h"

#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "splitView.h"
#include "palette.h"      // palette_band_height and palette_render — the band the topbar grows
#include "fileBrowser.h"
#include "msgQueue.h"
#include "patchWrite.h"   // write_database_to_file() — split out of graphics.c, which is not in this build
#include "misc.h"         // recent_files_add()
#include "alertDialog.h"       // show_alert() on a failed write
#include "synthlibPopups.h"    // synthlib_popups_render() — draws the alert dialog and the browsers
#include "g2AppStubs.h"
#include "g2Patch.h"
#include "g2Draw.h"

// The application's canvas grey (graphics.c render_frame()), so the strip reads as a piece of the
// editor rather than as a debug surface that happens to be switched on.
#define BACKGROUND_GREY    (0.8)

// Same font the application loads, and by the same absolute path — it is a system file, not a bundle
// resource, which is the one reason fonts are not a problem for a plug-in. A plug-in's bundle is not
// the app's, so anything loaded relative to it would have to be found all over again.
#define FONT_PATH     "/System/Library/Fonts/Supplemental/Arial.ttf"
#define FONT_PRELOAD_SIZE    (72.0)

static bool gFontReady = false;

// notes §3
static void g2_remember_file_path(const char * path) {
    if ((path == NULL) || (path[0] == '\0') || (gSavedPatchPath[gSlot] == path)) {
        return;
    }
    COPY_STRING(gSavedPatchPath[gSlot], path);
}

// What the file browser hands back. The application's equivalent goes through its own loader, which
// carries an online branch and pulls in GLFW; the plug-in already has its own in g2Patch.c.
static void g2_on_file_chosen(const char * path) {
    // A patch goes into the selected slot; a performance fills all four. Either way the loader
    // records the path as Save's target and sets the names, as the application's own loader does.
    tG2FileKind kind = g2_plugin_open_file(path, gSlot);

    if (kind == eG2FileFailed) {
        return;
    }
    sound_engine_update_from_patch();

    // The same event as in the application: the file just opened is what File > Open Recent lists.
    recent_files_add(path);

    {
        const char * leaf = strrchr(path, '/');

        g2_menu_set_loaded_patch_name((leaf != NULL) ? (leaf + 1) : path);
    }
}

// notes §4
static void g2_on_file_saved(const char * path) {
    if ((path == NULL) || (path[0] == '\0')) {
        return;     // Cancelled
    }
    LOG_INFO("Saving file: %s", path);

    // IN PERFORMANCE MODE, THE PERFORMANCE: all four slots and the performance settings, as the
    // application's offline save does (graphics.c). write_perf_to_file() was linked in and never
    // reached, because the plug-in held one slot and had no performance to write.
    if (gGlobalSettings.perfMode == 1) {
        if (write_perf_to_file(path) != EXIT_SUCCESS) {
            show_alert("Save Performance", "The performance could not be written. Check the folder is writable.");
            return;
        }

        if (gSavedPerfPath != path) {
            COPY_STRING(gSavedPerfPath, path);
        }
        recent_files_add(path);
        return;
    }

    if (write_database_to_file(path, gSlot) != EXIT_SUCCESS) {
        show_alert("Save Patch", "The patch could not be written. Check the folder is writable.");
        return;
    }
    set_patch_name_from_filename(gSlot, path);
    g2_remember_file_path(path);
    recent_files_add(path);     // A save lists the file too, exactly as an open does

    {
        const char * leaf = strrchr(path, '/');

        g2_menu_set_loaded_patch_name((leaf != NULL) ? (leaf + 1) : path);
    }
}

// THE THEME, KEPT so the topbar can grow. The application holds its own copy in graphics.c for the
// same reason: configure_synthlib_theme() takes the struct by value and there is nothing to read it
// back with, so re-applying one field means owning all of them.
static tSynthLibTheme gPluginTheme;

// notes §5
void apply_top_bar_height(void) {
    gPluginTheme.topBarHeight = MENU_BAR_HEIGHT + G2_PLUGIN_TOPBAR_HEIGHT + palette_band_height();
    configure_synthlib_theme(gPluginTheme);
    synthlib_request_redraw();
}

// The view's way in: it is Objective-C and knows its document only as a pointer, and selecting one
// is all it needs - every name the drawing and input code uses then refers to that instance.
void g2_draw_enter(void * doc) {
    g2_document_select((tG2Document *)doc);
}

void g2_draw_init(void) {
    // The same session-wide drawing state the application sets from synthlibWindow.c. Shared
    // rather than repeated, so the plug-in and the application cannot drift apart on it.
    render_backend_init();

    // The renderer asks the host application what its colours mean rather than including that app's
    // defs.h, so it has to be told before anything is drawn — exactly as init_graphics() does. The
    // values come from synthlibDefs.h so the plug-in and the application cannot drift apart.
    gPluginTheme = (tSynthLibTheme){
        // notes §6
        .topBarHeight   = MENU_BAR_HEIGHT + G2_PLUGIN_TOPBAR_HEIGHT,
        .orange1        = (tRgb)RGB_ORANGE_1,
        .orange2        = (tRgb)RGB_ORANGE_2,
        .greenOn        = (tRgb)RGB_GREEN_ON,
        .backgroundGrey = (tRgb)RGB_BACKGROUND_GREY,
    };

    configure_synthlib_theme(gPluginTheme);

    // SynthLib's popups and menu bar ask the host where the pointer is rather than reaching for a
    // window — the same injection init_graphics() performs. g2Input.c answers it from the host's
    // events, so the context-menu system works here unchanged.
    synthlib_host_init((tSynthLibHost){
        .mouseCoord = get_global_gui_scaled_mouse_coord,
    });

    // notes §7
    g2_menu_init();

    // ONLY IF NOTHING IS LOADED. This runs when the editor view is first created, which is AFTER the
    // processor has loaded its patch — so calling it unconditionally WIPED that patch the moment the
    // window was opened. Defaults are for an empty plug-in, not for one that already has something.
    if (slot_has_modules(0) == false) {
        init_patch(0);
    }

    // Two panes: the Voice Area and the FX Area, with a draggable bar between them — the same
    // arrangement the application starts in.
    split_view_init();

    // notes §8
    topbar_init_controls();

    gFontReady = preload_glyph_textures(FONT_PATH, FONT_PRELOAD_SIZE);
}

void g2_draw_frame(int pixelWidth, int pixelHeight, double backingScale) {
    double pointWidth  = 0.0;
    double pointHeight = 0.0;

    if ((pixelWidth <= 0) || (pixelHeight <= 0) || (backingScale <= 0.0)) {
        return;
    }
    // Logical units, once gGlobalGuiScale is set below. Named point* historically; they are the
    // canvas's own coordinate space, which is what every render call below expects.
    (void)backingScale;

    // notes §9
    render_backend_set_surface(pixelWidth, pixelHeight);

    // notes §10
    set_exclusive_button_highlight(topbarSlotAId, topbarSlotDId, (tTopbarControlId)((uint32_t)topbarSlotAId + gSlot));
    set_exclusive_button_highlight(topbarVariation1Id, topbarVariationInitId,
                                   (tTopbarControlId)((uint32_t)topbarVariation1Id + gPatchDescr[gSlot].activeVariation));

    set_render_width(pixelWidth);
    set_render_height(pixelHeight);

    // notes §11
    gGlobalGuiScale = (double)pixelWidth / (TARGET_FRAME_BUFF_WIDTH / 2.0);

    pointWidth      = (double)pixelWidth / gGlobalGuiScale;
    pointHeight     = (double)pixelHeight / gGlobalGuiScale;

    render_backend_clear((tRgb){BACKGROUND_GREY, BACKGROUND_GREY, BACKGROUND_GREY});

    // notes §12
    {
        tMessageContent msg = {0};

        if (g2_take_gui_message(&msg) == true) {
            // The same four cases the application's drain handles (graphics.c). Only the read one
            // used to be here, so File > Save As and File > Save posted a message that this drain
            // took off the queue and threw away — the dialogue simply never appeared.
            switch (msg.cmd) {
                case eRspShowOpenRead:
                    // Where it opens and what it remembers are set up once at start-up by
                    // load_saved_settings() (persistence.c), exactly as in the application.
                    open_file_browser_read(g2_on_file_chosen);
                    break;

                case eRspOpenPath:
                    // File > Open Recent. Through the same callback as the browser, so a recent
                    // open settles Save's target and re-orders the list by one route, not two.
                    g2_on_file_chosen(msg.patchFileData.filePath);
                    break;

                case eRspShowOpenWrite:
                {
                    // File > Save As. The default name comes from the patch or performance name,
                    // the way the application builds it.
                    char patchName[CLAVIA_NAME_SIZE + 1]   = {0};
                    char defaultName[CLAVIA_NAME_SIZE + 6] = {0};   // name (16) + extension (5) + null

                    if (gGlobalSettings.perfMode == 1) {
                        COPY_STRING(patchName, gGlobalSettings.perfName);
                    } else {
                        COPY_STRING(patchName, gGlobalSettings.slot[gSlot].patchName);
                    }

                    if (patchName[0] != '\0') {
                        snprintf(defaultName, sizeof(defaultName), "%s.%s", patchName,
                                 (gGlobalSettings.perfMode == 1) ? "prf2" : "pch2");
                    } else {
                        snprintf(defaultName, sizeof(defaultName), "%s",
                                 (gGlobalSettings.perfMode == 1) ? "performance.prf2" : "patch.pch2");
                    }
                    open_file_browser_write(g2_on_file_saved, defaultName);
                    break;
                }

                case eRspSaveToCurrentPath:

                    // File > Save: straight back to the remembered path, no browser. Re-checked
                    // here rather than trusted from the menu, for the application's reason — the
                    // drain runs a frame or more after the click.
                    {
                        char * target = (gGlobalSettings.perfMode == 1) ? gSavedPerfPath : gSavedPatchPath[gSlot];

                        if (target[0] == '\0') {
                            open_file_browser_write(g2_on_file_saved,
                                                    (gGlobalSettings.perfMode == 1) ? "performance.prf2" : "patch.pch2");
                        } else {
                            g2_on_file_saved(target);
                        }
                    }
                    break;

                default:
                    break;
            }
        }
    }

    sound_engine_update_from_patch();

    // The click regions must be cleared each frame because the renderer registers one for every
    // module, dial and connector as it draws — letting them accumulate would grow without limit for
    // as long as the window is open.
    clear_click_regions();

    // NO LONGER PINNED TO SLOT A. An instance holds all four slots (the document, globalVars.h), which
    // is what performance mode needs, and the top bar's A-D buttons choose between them exactly as the
    // application's do; this used to force gSlot back to 0 on every frame.

    // notes §13
    split_view_apply();

    {
        tLocation focusLocation = gLocation;
        uint32_t  focusPane     = split_view_focused_pane();

        for (uint32_t pane = 0; pane < module_pane_count(); pane++) {
            set_module_pane(pane);
            gLocation = (tLocation)split_view_location_for_pane(pane);
            module_pane_clip_begin();
            render_modules();
            render_cables();
            module_pane_clip_end();
        }

        set_module_pane(focusPane);
        gLocation = focusLocation;
    }

    render_split_bar();
    render_pane_scrollbars();

    // The cable being dragged, if any. Drawn AFTER the settled ones, as render_frame() does — it is
    // the thing under the pointer and belongs on top. Without it a cable drag is invisible until it
    // lands, which reads as nothing happening at all.
    if (gCableDrag.active == true) {
        tModule * from = get_module(gCableDrag.fromModuleKey);

        if (from != NULL) {
            tCableColour dragColour = cable_colour_for_connector_type(
                from->connector[gCableDrag.fromConnectorIndex].type);

            set_rgb_colour(gCableColourMap[dragColour]);
            render_cable_from_to(from->connector[gCableDrag.fromConnectorIndex], gCableDrag.toConnector, 4.0);
        }
    }

    // notes §14
    render_top_bar();

    // notes §15
    palette_render();

    // The morph group dials that sit at the right-hand end of the bar — Wheel, Vel, Keyb, Aft.Tch
    // and the rest. A separate function in the application too, and already linked here.
    render_morph_groups();

    render_menu_bar(gPluginMenuBar, g2_menu_bar_rect(pointWidth));

    // notes §16
    synthlib_popups_render();

    if (gFontReady == false) {
        // A patch of unreadable modules and a patch of missing font look far too similar to leave
        // to chance.
        set_rgb_colour((tRgb){0.7, 0.1, 0.1});
        render_rectangle(mainArea, (tRectangle){{0.0, 0.0}, {pointWidth, 6.0}});
    }
    // LAST, and required: the drawing above only QUEUED geometry. The caller flushes the GL
    // context straight after this returns, which presents whatever has been submitted — so
    // without this the plug-in's editor draws nothing. See utilsGraphics.h.
    render_backend_flush();
}
