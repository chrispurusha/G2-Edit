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

#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "g2GlDraw.h"

// The application's canvas grey (graphics.c render_frame()), so the strip reads as a piece of the
// editor rather than as a debug surface that happens to be switched on.
#define BACKGROUND_GREY    (0.8)

// Same font the application loads, and by the same absolute path — it is a system file, not a bundle
// resource, which is the one reason fonts are not a problem for a plug-in. A plug-in's bundle is not
// the app's, so anything loaded relative to it would have to be found all over again.
#define FONT_PATH     "/System/Library/Fonts/Supplemental/Arial.ttf"
#define FONT_PRELOAD_SIZE    (72.0)

// Shown until the File menu is used. Says WHICH patch rather than just "a patch": a plug-in playing
// the wrong thing and a plug-in playing nothing look the same from outside.
#define G2_BUILTIN_PATCH_LABEL    "Built-in patch"

static bool gFontReady = false;

void g2_gl_draw_init(void) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

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

    // What synthlibScale.c's synthlib_scale_update() does, minus the part that needs GLFW. It is
    // repeated rather than called because that file's two OTHER functions call glfwGetWindowContentScale,
    // so linking it would drag GLFW into the plug-in. Splitting those two out is the next refactor
    // step — see plugin-gui-notes.md — and then this becomes one call.
    glViewport(0, 0, pixelWidth, pixelHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, (GLdouble)pixelWidth, (GLdouble)pixelHeight, 0.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

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

    glClearColor((GLfloat)BACKGROUND_GREY, (GLfloat)BACKGROUND_GREY, (GLfloat)BACKGROUND_GREY, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

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
    sound_engine_update_from_patch();

    // The click regions must be cleared each frame because the renderer registers one for every
    // module, dial and connector as it draws — letting them accumulate would grow without limit for
    // as long as the window is open.
    clear_click_regions();

    // Which slot and area to draw. The plug-in loads its patch into slot 0, and the Voice Area is
    // where the interesting part of a patch lives; the FX area is a second pane in the application
    // and would need the pane machinery to show here.
    gSlot     = 0;
    gLocation = locationVa;

    set_module_pane_count(1);
    set_module_pane(0);

    render_modules();
    render_cables();

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

    // Chrome over the canvas. The reserved topbar band is drawn as a plain strip for now: an empty
    // gap would read as a rendering fault, whereas a filled bar reads as a bar with nothing in it.
    {
        tRectangle topbar = g2_topbar_rect(pointWidth);

        set_rgb_colour((tRgb)RGB_GREY_5);
        render_rectangle(mainArea, topbar);

        set_rgb_colour((tRgb)RGB_GREY_3);

        {
            const char * patch = g2_menu_loaded_patch_name();

            render_text(mainArea, (tRectangle){{10.0, topbar.coord.y + 14.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}},
                        (patch != NULL) ? patch : G2_BUILTIN_PATCH_LABEL);
        }
    }

    render_menu_bar(gPluginMenuBar, g2_menu_bar_rect(pointWidth));

    // LAST, so an open menu is drawn over everything it overlaps.
    render_context_menu();

    if (gFontReady == false) {
        // A patch of unreadable modules and a patch of missing font look far too similar to leave
        // to chance.
        set_rgb_colour((tRgb){0.7, 0.1, 0.1});
        render_rectangle(mainArea, (tRectangle){{0.0, 0.0}, {pointWidth, 6.0}});
    }
}
