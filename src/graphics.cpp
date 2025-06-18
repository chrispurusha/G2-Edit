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
#include "globalVars.h"

static FT_Library      gLibrary     = {0};
static FT_Face         gFace        = {0};
static pthread_mutex_t gReDrawMutex = {0};

static void re_draw_mutex_lock(void) {
    // TODO: implement a generic utility function for this, passing the mutex?
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
    // TODO: implement a generic utility function for this, passing the mutex?
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

void set_window_title(const char * title) {
    const char * filename = strrchr(title, '/');
    char         newTitle[100];

    strcpy(newTitle, WINDOW_TITLE);
    strcat(newTitle, " - ");
    strcat(newTitle, filename + 1);
    glfwSetWindowTitle(gWindow, newTitle);
}

void error_callback(int error, const char * description) {
    LOG_ERROR("GLFW error [%d]: %s\n", error, description);
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
                set_rgb_colour(RGB_CONTEXT_MENU_GREEN);
            } else {
                set_rgb_colour(RGB_GREY_3);    // Background
            }
            render_rectangle(mainArea, menuItem);

            set_rgb_colour(RGB_GREY_9);    // White text
            render_text(mainArea, {{gContextMenu.coord.x + 5, gContextMenu.coord.y + 5 + yOffset}, {BLANK_SIZE, itemHeight}}, gContextMenu.items[i].label);
            yOffset += itemHeight + (5 * 2);
        }
    }
}

void render_scrollbars(GLFWwindow * window) {
    double renderWidth  = get_render_width() / GLOBAL_GUI_SCALE;
    double renderHeight = get_render_height() / GLOBAL_GUI_SCALE;

    // Scrollbar background
    set_rgb_colour(RGB_GREY_7);
    gScrollState.xRectangle = render_rectangle(mainArea, {{0.0, renderHeight - SCROLLBAR_WIDTH}, {renderWidth - SCROLLBAR_MARGIN, SCROLLBAR_WIDTH}});
    gScrollState.yRectangle = render_rectangle(mainArea, {{renderWidth - SCROLLBAR_WIDTH, 0.0}, {SCROLLBAR_WIDTH, renderHeight - SCROLLBAR_MARGIN}});

    // Bottom right box
    set_rgb_colour(RGB_BACKGROUND_GREY);
    render_rectangle(mainArea, {{renderWidth - SCROLLBAR_WIDTH, renderHeight - SCROLLBAR_WIDTH}, {SCROLLBAR_WIDTH, SCROLLBAR_WIDTH}});

    // Scroll indicator blocks
    set_rgb_colour(RGB_GREY_9);
    render_rectangle(mainArea, {{renderWidth - SCROLLBAR_WIDTH, gScrollState.yBar - (SCROLLBAR_LENGTH / 2.0)}, {SCROLLBAR_WIDTH, SCROLLBAR_LENGTH}});
    render_rectangle(mainArea, {{gScrollState.xBar - (SCROLLBAR_LENGTH / 2.0), renderHeight - SCROLLBAR_WIDTH}, {SCROLLBAR_LENGTH, SCROLLBAR_WIDTH}});
}

void render_top_bar(void) {
    tRectangle rectangle = {0};

    set_rgb_colour(RGB_GREY_5);
    render_rectangle_with_border(mainArea, {{0.0, 0.0}, {(get_render_width() / GLOBAL_GUI_SCALE) - SCROLLBAR_MARGIN, TOP_BAR_HEIGHT}});

    set_rgb_colour(RGB_BLACK);
    render_text(mainArea, {{400.0, 43.0}, {NULL, STANDARD_TEXT_HEIGHT}}, "Variation");

    for (int i = 0; i < array_size_main_button_array(); i++) {
        set_rgb_colour(gMainButtonArray[i].backgroundColour);

        rectangle = {gMainButtonArray[i].coord, {get_text_width(gMainButtonArray[i].text, STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_TEXT_HEIGHT}};

        switch (gMainButtonArray[i].anchor) {
            case anchorTopRight:
            {
                rectangle.coord.x = (get_render_width() / GLOBAL_GUI_SCALE) + gMainButtonArray[i].coord.x;
                break;
            }
            default:
            {
                break;
            }
        }
        gMainButtonArray[i].rectangle = draw_button(mainArea, rectangle, gMainButtonArray[i].text);
    }
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
    gLocation                                     = locationVa;
    gMainButtonArray[vaButtonId].backgroundColour = (tRgb)RGB_GREEN_ON;
    gMainButtonArray[fxButtonId].backgroundColour = (tRgb)RGB_BACKGROUND_GREY;
}

void setup_render_context(void) {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, get_render_width(), get_render_height(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

void init_graphics(void) {
    int           fbWidth      = 0;
    int           fbHeight     = 0;
    int           windowWidth  = 0;
    int           windowHeight = 0;
    GLFWmonitor * monitor      = NULL;
    float         xScale       = 1;
    float         yScale       = 1;

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    register_glfw_wake_cb(wake_glfw);
    register_full_patch_change_notify_cb(notify_full_patch_change);

    monitor = glfwGetPrimaryMonitor();

    glfwGetMonitorContentScale(monitor, &xScale, &yScale);
    //xScale *= 0.4;
    //yScale *= 0.4;
    windowWidth  = TARGET_FRAME_BUFF_WIDTH / xScale;
    windowHeight = TARGET_FRAME_BUFF_HEIGHT / yScale;

    gWindow = glfwCreateWindow(windowWidth, windowHeight, WINDOW_TITLE, NULL, NULL);

    if (!gWindow) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwSetWindowSizeLimits(gWindow, windowWidth / GLOBAL_GUI_SCALE, windowHeight / GLOBAL_GUI_SCALE, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwSetWindowAspectRatio(gWindow, windowWidth, windowHeight);

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
        LOG_ERROR("Failed to preload glyph textures\n");
    }
    int renderWidth  = 0;
    int renderHeight = 0;
    //glfwGetWindowSize(gWindow, &renderWidth, &renderHeight);
    glfwGetFramebufferSize(gWindow, &renderWidth, &renderHeight);
    set_render_width(renderWidth);
    set_render_height(renderHeight);

    setup_render_context();
}

void read_file_into_memory_and_process(const char * filepath) {
    int64_t   byteOffset = 0;
    int64_t   fileSize   = 0;
    FILE *    file       = NULL;
    uint8_t * buff       = NULL;
    size_t    readSize   = 0;
    uint8_t   type       = 0;
    uint32_t  readCrc    = 0;
    uint32_t  calcCrc    = 0;

    file = fopen(filepath, "rb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        return;
    }
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    rewind(file);

    buff = (uint8_t *)malloc(fileSize);

    if (buff == NULL) {
        LOG_ERROR("Memory allocation failed\n");
        fclose(file);
        return;
    }
    readSize = fread(buff, 1, fileSize, file);

    if (readSize != fileSize) {
        LOG_ERROR("Failed to read entire file\n");
        free(buff);
        fclose(file);
        return;
    }

    for (int64_t i = 0; i < fileSize; i++) {
        if (buff[i] == 0x00) {
            byteOffset = i + 1;
            break;
        }
    }

    readCrc = buff[fileSize - 2] << 8 | buff[fileSize - 1];
    calcCrc = calc_crc16(buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));

    if (readCrc == calcCrc) {
        //uint8_t version = buffer[byteOffset++];
        byteOffset++; // Version
        type = buff[byteOffset++];
        //LOG_DEBUG("Version %u\n", version);
        //LOG_DEBUG("Type %u\n", type);

        database_clear_cables();
        database_clear_modules();

        if (type == 0) {
            parse_patch(gSlot, buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));  // TODO: parse_patch should really be in a commonly accessible source file, for file or USB access
        } // 1 = performance
    } else {
        LOG_WARNING("CRC check failed\n");
    }
    free(buff);
    fclose(file);
}

void check_action_flags(void) {
    if (gShowOpenFileReadDialogue == true) { // TODO: move to a function
        char * path = open_file_dialogue();

        if (path != NULL) {
            LOG_INFO("\n\nSelected file: %s\n", path);

            set_window_title(path);

            read_file_into_memory_and_process(path);

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
            render_morph_groups();
            render_scrollbars(gWindow);
            render_context_menu();
            //Debug only
            //{
            //    double x        = 0.0;
            //    double y        = 0.0;
            //    int    fbWidth  = 0;
            //    int    fbHeight = 0;
            //    glfwGetCursorPos(gWindow, &x, &y);
            //    glfwGetFramebufferSize(gWindow, &fbWidth, &fbHeight);
            //
            //    x /= fbWidth;
            //    y /= fbHeight;
            //    x *= TARGET_FRAME_BUFF_WIDTH;
            //    y *= TARGET_FRAME_BUFF_HEIGHT;
            //
            //    set_rgb_colour(RGB_BLACK);
            //    render_line(mainArea, {x, y - 10}, {x, y + 10}, 3);
            //    render_line(mainArea, {x - 10, y}, {x + 10, y}, 3);
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
            glfwWaitEvents(); // Might have to wait on timeout and not use the empty event post, since indication that it  can't be called from other thread
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
