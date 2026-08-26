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

// What gets drawn into the plug-in's OpenGL surface — plain C, no Cocoa.
//
// THIS NOW USES THE APPLICATION'S OWN RENDERER. Everything below draws through SynthLib's
// utilsGraphics.c — the same render_rectangle(), render_text() and set_rgb_colour() the editor
// canvas is built from — rather than through raw GL calls of its own. That is the point of the
// exercise: the drawing code was never the part tied to GLFW, and this file is the evidence.
//
// The split from g2GlView.m matters more than the contents. That file owns the NSOpenGLView, the
// context and the host's resize notifications; this one owns pixels and knows nothing about who is
// hosting it. It is C rather than Objective-C because nothing here needs a runtime.

#define GL_SILENCE_DEPRECATION    1

#include <OpenGL/gl.h>

#include "sysIncludes.h"
// defs.h BEFORE synthlibDefs.h — it defines G2_EDIT, and synthlibDefs.h gates TOP_BAR_HEIGHT, the
// colour palette and several layout constants on it. Included the other way round, TOP_BAR_HEIGHT
// silently becomes 0.0 (the non-G2 branch), which put the module band 80 units too high, hidden
// behind the top bar and with the margin between them swallowed.
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
#include "fileBrowser.h"
#include "msgQueue.h"
#include "g2AppStubs.h"
#include "g2Patch.h"
#include "g2GlDraw.h"

// The application's canvas grey (graphics.c render_frame()), so the strip reads as a piece of the
// editor rather than as a debug surface that happens to be switched on.
#define BACKGROUND_GREY    (0.8)

// Same font the application loads, and by the same absolute path — it is a system file, not a bundle
// resource, which is the one reason fonts are not a problem for a plug-in. A plug-in's bundle is not
// the app's, so anything loaded relative to it would have to be found all over again.
#define FONT_PATH     "/System/Library/Fonts/Supplemental/Arial.ttf"
#define FONT_PRELOAD_SIZE    (72.0)

static bool gFontReady = false;

// What the file browser hands back. The application's equivalent goes through its own loader, which
// carries an online branch and pulls in GLFW; the plug-in already has its own in g2Patch.c.
static void g2_on_file_chosen(const char * path) {
    if ((path == NULL) || (g2_plugin_load_patch(path, 0) == false)) {
        return;
    }
    sound_engine_update_from_patch();

    // The topbar reads the patch name out of gGlobalSettings, not from us — so set it the way the
    // application does when it opens a file, rather than only remembering it for ourselves.
    set_patch_name_from_filename(0, path);

    {
        const char * leaf = strrchr(path, '/');

        g2_menu_set_loaded_patch_name((leaf != NULL) ? (leaf + 1) : path);
    }
}

void g2_gl_draw_init(void) {
    // The same session-wide drawing state the application sets from synthlibWindow.c. Shared
    // rather than repeated, so the plug-in and the application cannot drift apart on it.
    render_backend_init();

    // The renderer asks the host application what its colours mean rather than including that app's
    // defs.h, so it has to be told before anything is drawn — exactly as init_graphics() does. The
    // values come from synthlibDefs.h so the plug-in and the application cannot drift apart.
    configure_synthlib_theme((tSynthLibTheme){
        // The canvas starts BELOW the menu bar and the reserved topbar band. topBarHeight is how the
        // renderer is told that, and it is what keeps modules from being drawn underneath them.
        // NOT the application's value: its bar carries slot, performance and clock controls that a
        // plug-in has no use for, so the reserved band here is smaller. Reserving it now means adding
        // the topbar's controls later does not shift the whole patch down.
        .topBarHeight   = MENU_BAR_HEIGHT + G2_PLUGIN_TOPBAR_HEIGHT,
        .orange1        = (tRgb)RGB_ORANGE_1,
        .orange2        = (tRgb)RGB_ORANGE_2,
        .greenOn        = (tRgb)RGB_GREEN_ON,
        .backgroundGrey = (tRgb)RGB_BACKGROUND_GREY,
    });

    // SynthLib's popups and menu bar ask the host where the pointer is rather than reaching for a
    // window — the same injection init_graphics() performs. g2Input.c answers it from the host's
    // events, so the context-menu system works here unchanged.
    synthlib_host_init((tSynthLibHost){
        .mouseCoord = get_global_gui_scaled_mouse_coord,
    });

    // A brand-new empty patch FIRST. gPatchDescr is otherwise all zeroes, and the pane divider's
    // position is patch data (gPatchDescr[].barPosition) — zero meaning "Voice Area takes no
    // height", which pinned the divider to the top of an empty window. init_patch() sets the same
    // 300 the application uses for a new patch, deliberately showing both areas.
    //
    // A patch loaded afterwards carries its own barPosition and overwrites this, exactly as in the
    // application.
    // Tells appMenuBar.c there can never be a G2 attached, so its bank entries are omitted.
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

    // The top bar's controls take their colours from here. WITHOUT IT every control is drawn with a
    // zeroed tRgb — which is BLACK — so Undo/Redo and the A-D slot buttons came out as black
    // rectangles. init_graphics() calls it for the same reason; nothing about the bar works until
    // it has.
    topbar_init_controls();

    gFontReady = preload_glyph_textures(FONT_PATH, FONT_PRELOAD_SIZE);
}

void g2_gl_draw_frame(int pixelWidth, int pixelHeight, double backingScale) {
    double pointWidth  = 0.0;
    double pointHeight = 0.0;

    if ((pixelWidth <= 0) || (pixelHeight <= 0) || (backingScale <= 0.0)) {
        return;
    }
    // Logical units, once gGlobalGuiScale is set below. Named point* historically; they are the
    // canvas's own coordinate space, which is what every render call below expects.
    (void)backingScale;

    // The viewport-and-projection half of synthlibScale.c's synthlib_scale_update(). That file
    // cannot be linked here — its two OTHER functions call glfwGetWindowContentScale, which would
    // drag GLFW into the plug-in — but the graphics half now lives in utilsGraphics.c, which the
    // plug-in already compiles, so it is shared rather than repeated. What stays below is the
    // scaling arithmetic, which genuinely does differ from the application's.
    render_backend_set_surface(pixelWidth, pixelHeight);

    set_render_width(pixelWidth);
    set_render_height(pixelHeight);

    // THE APPLICATION'S OWN SCALING FORMULA, and adopting it is what makes the plug-in show the same
    // field of view as the editor rather than a cropped corner of it.
    //
    // The whole UI is laid out in a fixed logical canvas TARGET_FRAME_BUFF_WIDTH/2 units wide (1280),
    // and gGlobalGuiScale maps that onto however many physical pixels there are. This used to be set
    // to the backing scale, which quietly redefined the logical canvas as 900 units — so roughly 70%
    // of the app's field of view, with larger patches running off the edge and no scrolling to
    // recover them.
    //
    // A consequence worth stating: coordinates below are now LOGICAL UNITS, not points. At a 900pt
    // window they are about 1.42 to the point. Everything the renderer draws — including the menu
    // bar's own MENU_BAR_HEIGHT — is in those units, which is exactly how the application treats
    // them, so the chrome scales with the canvas instead of staying a fixed pixel size.
    gGlobalGuiScale = (double)pixelWidth / (TARGET_FRAME_BUFF_WIDTH / 2.0);

    pointWidth      = (double)pixelWidth / gGlobalGuiScale;
    pointHeight     = (double)pixelHeight / gGlobalGuiScale;

    render_backend_clear((tRgb){BACKGROUND_GREY, BACKGROUND_GREY, BACKGROUND_GREY});

    // PUSH THE PATCH TO THE ENGINE, exactly as the application's render_frame() does (graphics.c).
    //
    // Turning a dial writes to the module database; the audio thread reads a parameter SNAPSHOT, and
    // nothing is heard until that snapshot is rebuilt. In the application a redraw is the event that
    // rebuilds it, and every edit causes a redraw — so doing it here gives the plug-in the same
    // behaviour, and covers every kind of edit rather than dials alone.
    //
    // Safe against process() rebuilding on the audio thread at the same moment: the snapshot's
    // writers were given a mutex (gParamsWriteMutex in soundEngine.c) when the mod wheel latency was
    // fixed, and the critical section is one struct copy.
    // Deferred menu actions, drained here for the same reason the application drains them in its
    // render loop: a browser must not be opened from inside a menu callback. See msg_send() in
    // g2AppStubs.c for how the message gets this far.
    {
        tMessageContent msg = {0};

        if (g2_take_gui_message(&msg) == true) {
            if (msg.cmd == eRspShowOpenRead) {
                // Where it opens and what it remembers are set up once at start-up by
                // load_saved_settings() (persistence.c), exactly as in the application.
                open_file_browser_read(g2_on_file_chosen);
            }
        }
    }

    sound_engine_update_from_patch();

    // The click regions must be cleared each frame because the renderer registers one for every
    // module, dial and connector as it draws — letting them accumulate would grow without limit for
    // as long as the window is open.
    clear_click_regions();

    // The plug-in loads its patch into slot 0; a plug-in instance is one patch, and the G2's
    // four-slot performance layout is a hardware notion with nothing to map onto here.
    gSlot = 0;

    // BOTH PANES, driven exactly as render_frame() drives them. render_modules()/render_cables()
    // read gLocation at their top, so the location is set around each pass rather than passed in —
    // the "mode rather than argument" style the pane machinery uses throughout. gLocation is put
    // back to the focused pane's afterwards, since that is what every other reader means by it.
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

    // THE APPLICATION'S OWN TOP BAR, not a second implementation of it. An earlier attempt here
    // drew a hand-picked subset — patch name, variations, cable toggles — and looked nothing like
    // the editor, which defeats the point: a plug-in that resembles the application is the whole
    // reason for reusing its renderer. The controls that describe hardware draw too and should:
    // "Offline" is the truthful state here, and the TX/RX lamps simply stay dark.
    render_top_bar();

    // The morph group dials that sit at the right-hand end of the bar — Wheel, Vel, Keyb, Aft.Tch
    // and the rest. A separate function in the application too, and already linked here.
    render_morph_groups();

    render_menu_bar(gPluginMenuBar, g2_menu_bar_rect(pointWidth));

    // LAST, so an open menu is drawn over everything it overlaps.
    // Above the canvas and the chrome, below nothing.
    render_file_browser();

    render_context_menu();

    if (gFontReady == false) {
        // A patch of unreadable modules and a patch of missing font look far too similar to leave
        // to chance.
        set_rgb_colour((tRgb){0.7, 0.1, 0.1});
        render_rectangle(mainArea, (tRectangle){{0.0, 0.0}, {pointWidth, 6.0}});
    }
}
