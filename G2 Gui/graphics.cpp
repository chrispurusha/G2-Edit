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
#include "fileDialogue.h"

void va_button(void);
void fx_button(void);
void open_read_file_button(void);

GLFWwindow *           gWindow                   = NULL;
bool                   gReDraw                   = true;
uint32_t               gLocation                 = locationVa;
tButton                gSelectVa                 = {NULL_RECTANGLE, "VA", va_button}; // TODO: put these select items in an array of structures
tButton                gSelectFx                 = {NULL_RECTANGLE, "FX", fx_button};
tButton                gSelectOpenReadFile       = {NULL_RECTANGLE, "Read File", open_read_file_button};
bool                   gShowOpenFileReadDialogue = false;

static FT_Library      gLibrary     = {0};
static FT_Face         gFace        = {0};
static pthread_mutex_t gReDrawMutex = {0};

extern tScrollState    gScrollState;
extern tContextMenu    gContextMenu;
extern tMessageQueue   gCommandQueue;
extern tCableDragging  gCableDrag;
extern tDialDragging   gDialDragging;
extern tModuleDragging gModuleDrag;

static void re_draw_mutex_lock(void) {
    // Todo: implement a generic utility function for this, passing the mutex?
    if (pthread_mutex_lock(&gReDrawMutex) != 0) {
        pthread_mutexattr_t attr = {0};

        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
        pthread_mutex_init(&gReDrawMutex, &attr);
        pthread_mutexattr_destroy(&attr);

        pthread_mutex_lock(&gReDrawMutex);
    }
}

static void re_draw_mutex_unlock(void) {
    // Todo: implement a generic utility function for this, passing the mutex?
    pthread_mutex_unlock(&gReDrawMutex);
}

void framebuffer_size_callback(GLFWwindow * window, int width, int height) {
    glViewport(0, 0, width, height);
    re_draw_mutex_lock();
    gReDraw = true;
    re_draw_mutex_unlock();
}

void window_close_callback(GLFWwindow * window) {
    re_draw_mutex_lock();
    gReDraw = false;
    re_draw_mutex_unlock();

    glfwSetFramebufferSizeCallback(gWindow, NULL);
    glfwSetWindowCloseCallback(gWindow, NULL);
    glfwSetKeyCallback(gWindow, NULL);
    glfwSetCharCallback(gWindow, NULL);
    glfwSetCursorPosCallback(gWindow, NULL);
    glfwSetMouseButtonCallback(gWindow, NULL);
    glfwSetScrollCallback(gWindow, NULL);

    glfwSetWindowShouldClose(gWindow, GLFW_TRUE);
    glfwPostEmptyEvent();
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

void open_read_file_button(void) {
    gShowOpenFileReadDialogue = true;
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
                set_rgb_colour({0.2, 0.6, 0.2});
            } else {
                set_rgb_colour({0.3, 0.3, 0.3});    // Background
            }
            render_rectangle(mainArea, menuItem);

            set_rgb_colour({0.9, 0.9, 0.9});    // White text
            render_text(mainArea, {{gContextMenu.coord.x + 5, gContextMenu.coord.y + 5 + yOffset}, {BLANK_SIZE, itemHeight}}, gContextMenu.items[i].label);
            yOffset += itemHeight + (5 * 2);
        }
    }
}

void render_scrollbars(GLFWwindow * window) {
    // Scrollbar background
    set_rgb_colour({0.7, 0.7, 0.7});
    render_rectangle(mainArea, {{(double)get_render_width() - SCROLLBAR_WIDTH, 0.0}, {SCROLLBAR_WIDTH, (double)get_render_height() - SCROLLBAR_MARGIN}});
    render_rectangle(mainArea, {{0.0, (double)get_render_height() - SCROLLBAR_WIDTH}, {(double)get_render_width() - SCROLLBAR_MARGIN, SCROLLBAR_WIDTH}});

    // Bottom right box
    set_rgb_colour(RGB_BACKGROUND_GREY);
    render_rectangle(mainArea, {{(double)get_render_width() - SCROLLBAR_WIDTH, (double)get_render_height() - SCROLLBAR_WIDTH}, {SCROLLBAR_WIDTH, SCROLLBAR_WIDTH}});

    // Scroll indicator blocks
    set_rgb_colour({0.9, 0.9, 0.9});
    render_rectangle(mainArea, {{(double)get_render_width() - SCROLLBAR_WIDTH, gScrollState.yBar - (SCROLLBAR_LENGTH / 2.0)}, {SCROLLBAR_WIDTH, SCROLLBAR_LENGTH}});
    render_rectangle(mainArea, {{gScrollState.xBar - (SCROLLBAR_LENGTH / 2.0), (double)get_render_height() - SCROLLBAR_WIDTH}, {SCROLLBAR_LENGTH, SCROLLBAR_WIDTH}});
}

void render_top_bar(void) {
    set_rgb_colour({0.5, 0.5, 0.5});
    render_rectangle_with_border(mainArea, {{0.0, 0.0}, {get_render_width() - SCROLLBAR_MARGIN, TOP_BAR_HEIGHT}});

    if (gLocation == locationVa) {
        set_rgb_colour({0.3, 0.7, 0.3});
    } else {
        set_rgb_colour(RGB_BACKGROUND_GREY);
    }
    gSelectVa.rectangle = draw_button(mainArea, {{400.0, 8.0}, {get_text_width(gSelectVa.text, STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_TEXT_HEIGHT}}, gSelectVa.text); // Todo - move coords into button definition

    if (gLocation == locationFx) {
        set_rgb_colour({0.3, 0.7, 0.3});
    } else {
        set_rgb_colour(RGB_BACKGROUND_GREY);
    }
    gSelectFx.rectangle = draw_button(mainArea, {{425.0, 8.0}, {get_text_width(gSelectFx.text, STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_TEXT_HEIGHT}}, gSelectFx.text);

    set_rgb_colour(RGB_BACKGROUND_GREY);
    gSelectOpenReadFile.rectangle = draw_button(mainArea, {{20.0, 8.0}, {get_text_width(gSelectOpenReadFile.text, STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_TEXT_HEIGHT}}, gSelectOpenReadFile.text);
}

void wake_glfw(void) {
    re_draw_mutex_lock();

    if (gReDraw == false) {
        gReDraw = true;
    }
    re_draw_mutex_unlock();
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
    glfwSetWindowCloseCallback(gWindow, window_close_callback);
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

void check_action_flags(void) {
    if (gShowOpenFileReadDialogue == true) { // Todo - move to a function
        const char * path = open_file_dialogue();

        if (path != NULL) {
            printf("Selected file: %s\n", path);
            free((void *)path);
        }
        gShowOpenFileReadDialogue = false;
    }
}

void do_graphics_loop(void) {
    bool reDraw = false;

    while (!glfwWindowShouldClose(gWindow)) {
        check_action_flags();

        re_draw_mutex_lock(); // Only really protecting the gap between setting redraw and clearing the global flag, may need re-think
        reDraw  = gReDraw;
        gReDraw = false;
        re_draw_mutex_unlock();

        if (reDraw == true) {
            glClearColor(0.8, 0.8, 0.8, 1.0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            render_modules();
            render_cables();

            if (gCableDrag.active == true) {
                tModule module = {0};
                read_module(gCableDrag.fromModuleKey, &module);
                set_rgb_colour(RGB_WHITE);
                render_cable_from_to(module.connector[gCableDrag.fromConnectorIndex], gCableDrag.toConnector);
            }
            render_top_bar();
            render_scrollbars(gWindow);
            render_context_menu();
            // Debug only
            //{
            //    double x = 0.0;
            //    double y = 0.0;
            //    glfwGetCursorPos(gWindow, &x, &y);
            //    set_rbg_colour(RGB_BLACK);
            //    render_line(mainArea, {x, y-10}, {x, y+10}, 3);
            //    render_line(mainArea, {x-10, y}, {x+10, y}, 3);
            //}

            // Swap buffers and look for events
            glfwSwapBuffers(gWindow);
        }

        if ((gModuleDrag.active == true) || (gCableDrag.active == true)) {
            double x = 0.0;
            double y = 0.0;
            glfwGetCursorPos(gWindow, &x, &y);
            cursor_pos(gWindow, x, y);  // Artificially do cursor_pos call for drag scrolling when cursor not moving
            glfwWaitEventsTimeout(0.016);
        } else {
            glfwWaitEvents(); // Todo: might have to wait on timeout and not use the empty event post, since indication that it can't be called from other thread
        }
    }
}

void clean_up_graphics(void) {
    // Clean up
    FT_Done_Face(gFace);
    FT_Done_FreeType(gLibrary);
    free_textures();

    glfwMakeContextCurrent(NULL);
    glfwDestroyWindow(gWindow);
    gWindow = NULL;
    glfwTerminate();
}

#ifdef __cplusplus
}
#endif
