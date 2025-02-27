/*
 * The G2 Editor application.
 *
 * Copyright (C) 2024 Chris Turner <chris_purusha@icloud.com>
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

// System header files
#include <math.h>

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#pragma clang diagnostic pop

#include "defs.h"
#include "msgQueue.h"
#include "usbComms.h"
#include "graphics.h"
#include "utilsGraphics.h"
#include "dataBase.h"
#include "moduleGraphics.h"


//static int           gRenderWidth  = 0;
//static int           gRenderHeight = 0;
static tScrollState gScrollState = {(SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false, (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN, false
};
//static double        gZoomFactor   = 1.0;

static FT_Library   gLibrary = {0};
static FT_Face      gFace    = {0};
static GLFWwindow * gWindow  = NULL;

static tDragging     gDragging = {0};
extern tMessageQueue gCommandQueue;

void framebuffer_size_callback(GLFWwindow * window, int width, int height) {
    glViewport(0, 0, width, height);
}

void error_callback(int error, const char * description) {
    fprintf(stderr, "Error [%d]: %s\n", error, description);
}

void set_xScrollBar(double x) {
    gScrollState.xBar = clamp_scroll_bar(x, get_render_width());
    set_x_scroll_percent(get_scroll_bar_percent(gScrollState.xBar, get_render_width()));
}

void set_yScrollBar(double y) {
    gScrollState.yBar = clamp_scroll_bar(y, get_render_height());
    set_y_scroll_percent(get_scroll_bar_percent(gScrollState.yBar, get_render_height()));
}

bool handle_scrollbar_click(tCoord coord) {
    tRectangle yScrollBar = {
        {(double)get_render_width() - SCROLLBAR_WIDTH,                         0.0},
        {SCROLLBAR_WIDTH,                              (double)get_render_height()}
    };

    tRectangle xScrollBar = {
        {                       0.0, (double)get_render_height() - SCROLLBAR_WIDTH},
        {(double)get_render_width(), SCROLLBAR_WIDTH                              }
    };

    if (within_rectangle(coord, yScrollBar)) {
        set_yScrollBar(coord.y);
        gScrollState.yBarDragging = true;
        return true;
    }
    if (within_rectangle(coord, xScrollBar)) {
        set_xScrollBar(coord.x);
        gScrollState.xBarDragging = true;
        return true;
    }

    return false;
}

bool handle_module_click(tCoord coord) {
    if (!within_rectangle(coord, module_area())) {
        return false;
    }

    reset_walk_module();
    tModule module = {0};;

    while (walk_next_module(&module)) {
        for (int i = 0; i < module.numParams; i++) {
            tParam * param = &module.param[0][i];

            if (!within_rectangle(coord, param->rectangle)) {
                continue;
            }

            if (param->type == paramTypeDial) {
                gDragging.index     = module.key.index;
                gDragging.location  = module.key.location;
                gDragging.variation = 0;
                gDragging.param     = i;
                gDragging.active    = true;
                return true;
            }
            else if (param->type == paramTypeToggle) {
                param->value = (param->value + 1) % param->range;
                write_module(module.key, &module);

                tMessageContent messageContent;
                messageContent.cmd       = eMsgCmdSetValue;
                messageContent.location  = module.key.location;
                messageContent.index     = module.key.index;
                messageContent.param     = i;
                messageContent.variation = 0;
                messageContent.value     = param->value;

                msg_send(&gCommandQueue, &messageContent);
                return true;
            }
        }
    }

    return false;
}

void mouse_button(GLFWwindow * window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT) {
        return;                                   // Ignore non-left clicks for now
    }
    int    width, height;
    tCoord coord = {0};

    glfwGetWindowSize(window, &width, &height);
    glfwGetCursorPos(window, &coord.x, &coord.y);

    // Scale coordinates
    coord.x = (coord.x * (double)get_render_width()) / (double)width;
    coord.y = (coord.y * (double)get_render_height()) / (double)height;

    printf("button=%d action=%d mods=%d\n", button, action, mods);

    if (action == GLFW_PRESS) {
        if (!handle_scrollbar_click(coord)) {
            handle_module_click(coord);
        }
    }
    else if (action == GLFW_RELEASE) {
        gScrollState.yBarDragging = false;
        gScrollState.xBarDragging = false;
        memset(&gDragging, 0, sizeof(gDragging));     // Reset dragging state
    }
}

void cursor_pos(GLFWwindow * window, double x, double y) {
    int             width          = 0;
    int             height         = 0;
    double          angle          = 0.0;
    uint32_t        value          = 0;
    tModule         module         = {0};
    tMessageContent messageContent = {0};


    // Scale x and y to match intended rendering window
    glfwGetWindowSize(window, &width, &height);
    x = (x * (double)width) / (double)width;
    y = (y * (double)height) / (double)height;

    if (gScrollState.yBarDragging == true) {
        set_yScrollBar(y);
    }
    else if (gScrollState.xBarDragging == true) {
        set_xScrollBar(x);
    }
    else if (gDragging.active == true) {
        read_module({gDragging.location, gDragging.index}, &module);
        switch (module.param[gDragging.variation][gDragging.param].type) {
            case paramTypeDial:
                angle = calculate_mouse_angle({x, y}, module.param[gDragging.variation][gDragging.param].rectangle);       // possible add half size
                value = angle_to_value(angle);
                module.param[gDragging.variation][gDragging.param].value = value;
                break;

            default:
                printf("Unknown module type %u\n", module.param[gDragging.variation][gDragging.param].type);
                exit(1);
        }

        write_module({gDragging.location, gDragging.index}, &module);       // Write new value into parameter

        messageContent.cmd       = eMsgCmdSetValue;
        messageContent.location  = gDragging.location;
        messageContent.index     = gDragging.index;     // module index (6?)
        messageContent.param     = gDragging.param;
        messageContent.variation = gDragging.variation;
        messageContent.value     = value;
        msg_send(&gCommandQueue, &messageContent);
    }
}

void scroll_event(GLFWwindow * window, double x, double y) {
    const double zoomIncrement = 0.05;  // Zoom sensitivity
    double       zoomFactor    = 0.0;
    //double     xEndMax = 0.0, yEndMax = 0.0;

    tCoord     mouseCoord = {0};
    tRectangle moduleArea = module_area(); // Get the module display area

    glfwGetCursorPos(window, &mouseCoord.x, &mouseCoord.y);

    //printf("Zoom = %f yEndMax = %f module area size = %f percent = %f\n", gZoomFactor, yEndMax, moduleArea.size.h, get_scroll_bar_percent(gScrollState.yBar, gRenderHeight));

    if (within_rectangle(mouseCoord, moduleArea)) {
        zoomFactor  = get_zoom_factor();
        zoomFactor += y * zoomIncrement;
        if (zoomFactor < 0.5) {
            zoomFactor = 0.5;
        }
        else if (zoomFactor > 2.0) {
            zoomFactor = 2.0;
        }
        set_zoom_factor(zoomFactor);
    }
}

void char_event(GLFWwindow * window, unsigned int value) {
    printf("char=%d\n", value);
}

void key_callback(GLFWwindow * window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    printf("key=%d scancode=%d action=%d mods=%d\n", key, scancode, action, mods);
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
}

void wake_glfw(void) {
    glfwPostEmptyEvent();
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
        glClearColor(0.8, 0.8, 0.8, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        render_modules();
        render_cables();
        render_top_bar();
        render_scrollbars(gWindow);

        // Swap buffers and look for events
        glfwSwapBuffers(gWindow);

        glfwWaitEvents();
        //glfwWaitEventsTimeout(0.016); // 60 hz if we wanted to do this way but uses more processing
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