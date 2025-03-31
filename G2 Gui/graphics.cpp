/*
 * The G2 Editor application.
 *
 * Copyright (C) 2025 Chris Turner <chris_purusha@icloud.com>
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

#include <ft2build.h>
#include FT_FREETYPE_H
#pragma clang diagnostic pop

#include "defs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "usbComms.h"
#include "graphics.h"
#include "utilsGraphics.h"
#include "mouseHandle.h"
#include "dataBase.h"
#include "moduleGraphics.h"

void va_button(void);
void fx_button(void);

GLFWwindow *           gWindow   = NULL;
bool                   gReDraw   = false;
uint32_t               gLocation = locationVa;
tButton                gSelectVa = {NULL_RECTANGLE, "VA", va_button}; // TODO: put these in an array of structures
tButton                gSelectFx = {NULL_RECTANGLE, "FX", fx_button};

static FT_Library      gLibrary = {0};
static FT_Face         gFace    = {0};

extern tScrollState    gScrollState;
extern tContextMenu    gContextMenu;
extern tMessageQueue   gCommandQueue;
extern tCableDragging  gCableDrag;
extern tDialDragging   gDialDragging;
extern tModuleDragging gModuleDrag;

void framebuffer_size_callback(GLFWwindow * window, int width, int height) {
    glViewport(0, 0, width, height);
    gReDraw = true;
}

void error_callback(int error, const char * description) {
    fprintf(stderr, "Error [%d]: %s\n", error, description);
}

void va_button(void) {
    gLocation = locationVa;
}

void fx_button(void) {
    gLocation = locationFx;
}

void render_context_menu(void) {
    if (!gContextMenu.active) {
        return;
    }
    double     size        = 0.0;
    double     largestSize = 0.0;
    tCoord     mouseCoord  = {0};
    tRectangle menuItem    = {0};
    double     itemHeight  = STANDARD_TEXT_HEIGHT;

    int        width, height;
    glfwGetWindowSize(gWindow, &width, &height);
    glfwGetCursorPos(gWindow, &mouseCoord.x, &mouseCoord.y);

    mouseCoord.x = (mouseCoord.x * (double)get_render_width()) / (double)width;
    mouseCoord.y = (mouseCoord.y * (double)get_render_height()) / (double)height;

    if (gContextMenu.items != NULL) {
        for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
            size = get_text_width(gContextMenu.items[i].label, itemHeight);

            if (size > largestSize) {
                largestSize = size;
            }
        }

        int yOffset = 0;

        for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
            menuItem = {{gContextMenu.coord.x, gContextMenu.coord.y + yOffset}, {largestSize + (5 * 2), itemHeight + (5 * 2)}};

            if (within_rectangle(mouseCoord, menuItem)) {
                set_rbg_colour({0.2, 0.6, 0.2});
            } else {
                set_rbg_colour({0.3, 0.3, 0.3});    // Background
            }
            render_rectangle(mainArea, menuItem);

            set_rbg_colour({0.9, 0.9, 0.9});    // White text
            render_text(mainArea, {{gContextMenu.coord.x + 5, gContextMenu.coord.y + 5 + yOffset}, {BLANK_SIZE, itemHeight}}, gContextMenu.items[i].label);
            yOffset += itemHeight + (5 * 2);
        }
    }
}

void render_scrollbars(GLFWwindow * window) {
    // Scrollbar background
    set_rbg_colour({0.7, 0.7, 0.7});
    render_rectangle(mainArea, {{(double)get_render_width() - SCROLLBAR_WIDTH, 0.0}, {SCROLLBAR_WIDTH, (double)get_render_height() - SCROLLBAR_MARGIN}});
    render_rectangle(mainArea, {{0.0, (double)get_render_height() - SCROLLBAR_WIDTH}, {(double)get_render_width() - SCROLLBAR_MARGIN, SCROLLBAR_WIDTH}});

    // Bottom right box
    set_rbg_colour(RGB_BACKGROUND_GREY);
    render_rectangle(mainArea, {{(double)get_render_width() - SCROLLBAR_WIDTH, (double)get_render_height() - SCROLLBAR_WIDTH}, {SCROLLBAR_WIDTH, SCROLLBAR_WIDTH}});

    // Scroll indicator blocks
    set_rbg_colour({0.9, 0.9, 0.9});
    render_rectangle(mainArea, {{(double)get_render_width() - SCROLLBAR_WIDTH, gScrollState.yBar - (SCROLLBAR_LENGTH / 2.0)}, {SCROLLBAR_WIDTH, SCROLLBAR_LENGTH}});
    render_rectangle(mainArea, {{gScrollState.xBar - (SCROLLBAR_LENGTH / 2.0), (double)get_render_height() - SCROLLBAR_WIDTH}, {SCROLLBAR_LENGTH, SCROLLBAR_WIDTH}});
}

void render_top_bar(void) {
    set_rbg_colour({0.5, 0.5, 0.5});
    render_rectangle_with_border(mainArea, {{0.0, 0.0}, {get_render_width() - SCROLLBAR_MARGIN, TOP_BAR_HEIGHT}});

    if (gLocation == locationVa) {
        set_rbg_colour({0.3, 0.7, 0.3});
    } else  {
        set_rbg_colour(RGB_BACKGROUND_GREY);
    }
    gSelectVa.rectangle = draw_button(mainArea, {{400.0, 10.0}, {get_text_width(gSelectVa.text, MAIN_MENU_TEXT_HEIGHT), MAIN_MENU_TEXT_HEIGHT}}, gSelectVa.text);

    if (gLocation == locationFx) {
        set_rbg_colour({0.3, 0.7, 0.3});
    } else  {
        set_rbg_colour(RGB_BACKGROUND_GREY);
    }
    gSelectFx.rectangle = draw_button(mainArea, {{425.0, 10.0}, {get_text_width(gSelectFx.text, MAIN_MENU_TEXT_HEIGHT), MAIN_MENU_TEXT_HEIGHT}}, gSelectFx.text);
}

void wake_glfw(void) {
    gReDraw = true;
    glfwPostEmptyEvent();
}

void notify_full_patch_change(void) {
    gLocation = locationVa;
}

void setup_render_context(void) {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, get_render_width(), get_render_height(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

void init_graphics(void) {
    tIntRectangle monitor      = {0};
    int           fbWidth      = 0;
    int           fbHeight     = 0;
    int           screenWidth  = 0;
    int           screenHeight = 0;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    register_glfw_wake_cb(wake_glfw);   //TODO - possible also register for callback on new module load, then reset scrollbars etc.
    register_full_patch_change_notify_cb(notify_full_patch_change);

    glfwGetMonitorWorkarea(glfwGetPrimaryMonitor(), &monitor.coord.x, &monitor.coord.y, &monitor.size.w, &monitor.size.h);
    screenWidth  = monitor.coord.x + monitor.size.w;
    screenHeight = monitor.coord.y + monitor.size.h;

    gWindow = glfwCreateWindow((screenWidth * 3) / 4, (screenHeight * 3) / 4, "G2 Editor", NULL, NULL);

    if (!gWindow) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwSetWindowSizeLimits(gWindow, (screenWidth * 1) / 4, (screenHeight * 1) / 4, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowAspectRatio(gWindow, screenWidth, screenHeight);

    glfwMakeContextCurrent(gWindow);

    glfwGetFramebufferSize(gWindow, &fbWidth, &fbHeight);
    glViewport(0, 0, fbWidth, fbHeight);

    glfwSetFramebufferSizeCallback(gWindow, framebuffer_size_callback);
    glfwSwapInterval(1);

    glfwSetKeyCallback(gWindow, key_callback);
    glfwSetCharCallback(gWindow, char_event);
    glfwSetCursorPosCallback(gWindow, cursor_pos);
    glfwSetMouseButtonCallback(gWindow, mouse_button);
    glfwSetScrollCallback(gWindow, scroll_event);

    FT_Init_FreeType(&gLibrary);
    FT_New_Face(gLibrary, "/System/Library/Fonts/Supplemental/Arial.ttf", 0, &gFace);
    FT_Set_Char_Size(gFace, 0, 48 * 64, 300, 300);

    // Preload glyph textures
    if (!preload_glyph_textures("/System/Library/Fonts/Supplemental/Arial.ttf", 72.0f)) {
        printf("Failed to preload glyph textures.\n");
    }
    int renderWidth  = 0;
    int renderHeight = 0;
    glfwGetWindowSize(gWindow, &renderWidth, &renderHeight);
    set_render_width(renderWidth);
    set_render_height(renderHeight);

    setup_render_context();
}

void do_graphics_loop(void) {
    while (!glfwWindowShouldClose(gWindow)) {
        if (gReDraw == true) {
            glClearColor(0.8, 0.8, 0.8, 1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            render_modules();
            render_cables();

            if (gCableDrag.active == true) {
                tModule module = {0};
                read_module(gCableDrag.fromModuleKey, &module);
                set_rbg_colour(RGB_WHITE);
                render_cable_from_to(module.connector[gCableDrag.fromConnectorIndex], gCableDrag.toConnector);
            }
            render_top_bar();
            render_scrollbars(gWindow);
            render_context_menu();

            // Swap buffers and look for events
            glfwSwapBuffers(gWindow);
            gReDraw = false;
        }

        if ((gModuleDrag.active == true) || (gCableDrag.active == true)) {
            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(gWindow, &x, &y);
            cursor_pos(gWindow, x, y);  // Artificially do cursor_pos call for drag scrolling
            glfwWaitEventsTimeout(0.016);
        } else {
            glfwWaitEvents();
        }
    }
}

void clean_up_graphics(void) {
    // Clean up
    FT_Done_Face(gFace);
    FT_Done_FreeType(gLibrary);
    free_textures();

    glfwDestroyWindow(gWindow);
    glfwTerminate();
}

#ifdef __cplusplus
}
#endif
