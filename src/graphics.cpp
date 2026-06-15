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

#include "math.h"
#include "defs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "protocol.h"
#include "usbComms.h"
#include "graphics.h"
#include "utilsGraphics.h"
#include "mouseHandle.h"
#include "dataBase.h"
#include "moduleGraphics.h"
#include "fileDialogue.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"

static FT_Library   gLibrary   = {0};
static FT_Face      gFace      = {0};
static _Atomic bool gNeedFocus = false;

void framebuffer_size_callback(GLFWwindow * window, int width, int height) {
    glViewport(0, 0, width, height);
    atomic_store(&gReDraw, true);
}

void window_close_callback(GLFWwindow * window) {
    atomic_store(&gReDraw, false);

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

void set_window_title(const char * filePath) {
    char         newTitle[100] = {0};
    const char * filename      = strrchr(filePath, '/');

    if (filename) {
        filename += 1;  // Skip the slash
    } else {
        filename = filePath;
    }
    snprintf(newTitle, sizeof(newTitle), "%s - %s", WINDOW_TITLE, filename);
    glfwSetWindowTitle(gWindow, newTitle);
}

void error_callback(int error, const char * description) {
    LOG_ERROR("GLFW error [%d]: %s\n", error, description);
}

void render_context_menu(void) {
    double     size        = 0.0;
    double     largestSize = 0.0;
    tCoord     mouseCoord  = {0};
    tRectangle menuItem    = {0};
    double     itemHeight  = STANDARD_TEXT_HEIGHT;
    uint32_t   columns     = (gContextMenu.columns > 1) ? gContextMenu.columns : 1;

    if (!gContextMenu.active) {
        return;
    }
    get_global_gui_scaled_mouse_coord(&mouseCoord);

    if (gContextMenu.items != NULL) {
        for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
            size = get_text_width(gContextMenu.items[i].label, itemHeight);

            if (size > largestSize) {
                largestSize = size;
            }
        }

        double computed = (largestSize + (5 * 2) > itemHeight) ? largestSize + (5 * 2) : itemHeight;
        double cellW    = (gContextMenu.cellWidth > 0.0) ? gContextMenu.cellWidth : computed;
        double cellH    = itemHeight + (5 * 2);

        for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
            int    col = (int)(i % columns);
            int    row = (int)(i / columns);
            double x   = gContextMenu.coord.x + col * cellW;
            double y   = gContextMenu.coord.y + row * cellH;
            menuItem = {{x, y}, {cellW, cellH}};

            set_rgb_colour(gContextMenu.items[i].colour);
            render_rectangle(mainArea, menuItem);

            set_rgb_colour(RGB_GREY_9);
            render_text(mainArea, {{x + 5, y + 5}, {BLANK_SIZE, itemHeight}}, gContextMenu.items[i].label);

            if (columns > 1) {
                set_rgb_colour(RGB_BLACK);
                render_line(mainArea, {x, y}, {x + cellW, y}, 1);
                render_line(mainArea, {x + cellW, y}, {x + cellW, y + cellH}, 1);
                render_line(mainArea, {x, y + cellH}, {x + cellW, y + cellH}, 1);
                render_line(mainArea, {x, y}, {x, y + cellH}, 1);
            }
        }

        for (int i = 0; gContextMenu.items[i].label != NULL; i++) {
            int    col = (int)(i % columns);
            int    row = (int)(i / columns);
            double x   = gContextMenu.coord.x + col * cellW;
            double y   = gContextMenu.coord.y + row * cellH;
            menuItem = {{x, y}, {cellW, cellH}};

            if (within_rectangle(mouseCoord, menuItem)) {
                set_rgb_colour(RGB_BLACK);
                render_line(mainArea, {menuItem.coord.x, menuItem.coord.y}, {menuItem.coord.x + menuItem.size.w, menuItem.coord.y}, 1);
                render_line(mainArea, {menuItem.coord.x + menuItem.size.w, menuItem.coord.y}, {menuItem.coord.x + menuItem.size.w, menuItem.coord.y + menuItem.size.h}, 1);
                render_line(mainArea, {menuItem.coord.x, menuItem.coord.y}, {menuItem.coord.x, menuItem.coord.y + menuItem.size.h}, 1);
                render_line(mainArea, {menuItem.coord.x, menuItem.coord.y + menuItem.size.h}, {menuItem.coord.x + menuItem.size.w, menuItem.coord.y + menuItem.size.h}, 1);
                set_rgb_colour(RGB_WHITE);
                render_line(mainArea, {menuItem.coord.x + 1, menuItem.coord.y + 1}, {(menuItem.coord.x + menuItem.size.w) - 1, menuItem.coord.y + 1}, 1);
                render_line(mainArea, {(menuItem.coord.x + menuItem.size.w - 1), menuItem.coord.y + 1}, {(menuItem.coord.x + menuItem.size.w) - 1, (menuItem.coord.y + menuItem.size.h) - 1}, 1);
                render_line(mainArea, {menuItem.coord.x + 1, menuItem.coord.y + 1}, {menuItem.coord.x + 1, (menuItem.coord.y + menuItem.size.h) - 1}, 1);
                render_line(mainArea, {menuItem.coord.x + 1, (menuItem.coord.y + menuItem.size.h) - 1}, {(menuItem.coord.x + menuItem.size.w) - 1, (menuItem.coord.y + menuItem.size.h) - 1}, 1);
            }
        }
    }
}

void render_scrollbars(GLFWwindow * window) {
    double renderWidth  = get_render_width() / gGlobalGuiScale;
    double renderHeight = get_render_height() / gGlobalGuiScale;

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
    tRectangle  rectangle                           = {0};
    char        patchNameCopy[CLAVIA_NAME_SIZE + 1] = {0};
    char        buff[32]                            = {0};
    tCommsState commsState                          = atomic_load(&gCommsState);
    char *      commsStateText                      = "Unknown";
    tRgb        commsStateColour                    = RGB_RED_7;
    tRgb        buttonBackgroundColour              = (tRgb)RGB_BACKGROUND_GREY;
    uint32_t    slot                                = atomic_load(&gSlot);
    uint32_t    variation                           = gPatchDescr[slot].activeVariation;
    int         voiceCount                          = 0;
    tModule     module                              = {0};

    set_rgb_colour(RGB_GREY_5);
    render_rectangle_with_border(mainArea, {{0.0, 0.0}, {(get_render_width() / gGlobalGuiScale) - SCROLLBAR_MARGIN, TOP_BAR_HEIGHT}});

    set_rgb_colour(RGB_BLACK);
    render_text(mainArea, {{400, 43}, {NULL, STANDARD_TEXT_HEIGHT}}, "Variation");

    patch_name_get(slot, patchNameCopy, sizeof(patchNameCopy));

    if (patchNameCopy[0] == '\0') {
        strncpy(patchNameCopy, "---", CLAVIA_NAME_SIZE);
    }
    //set_rgb_colour(RGB_BLACK);
    //render_text(mainArea, {{80, 43}, {NULL, STANDARD_TEXT_HEIGHT}}, "Patch Name");

    if (gPatchNameEdit.active && gPatchNameEdit.slot == slot) {
        // Show edit buffer with cursor
        char displayBuf[CLAVIA_NAME_SIZE + 2] = {0};
        snprintf(displayBuf, sizeof(displayBuf), "%s|", gPatchNameEdit.buffer);

        gPatchNameRectangle = draw_button(mainArea, {{20, 60}, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, displayBuf, (tRgb)RGB_WHITE);
    } else {
        gPatchNameRectangle = draw_button(mainArea, {{20, 60}, {get_text_width(LONGEST_PATCH_NAME, STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, patchNameCopy, (tRgb)RGB_BACKGROUND_GREY);
    }
    gPatchTypeRectangle  = draw_button(mainArea, {{170, 60}, {get_text_width("Sequencer", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, (char *)patchTypeStrMap[gPatchDescr[slot].category], (tRgb)RGB_BACKGROUND_GREY);
    gMonoPolyRectangle   = draw_button(mainArea, {{270, 60}, {get_text_width("Legato", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, (char *)monoPolyStrMap[gPatchDescr[slot].monoPoly], (tRgb)RGB_BACKGROUND_GREY);

    if (gPatchDescr[slot].monoPoly == monoPolyPoly) {
        voiceCount = gPatchDescr[slot].voiceCount + 1;
    } else {
        voiceCount = 1;
    }

    if ((gAssignedVoices[slot] == 0) || (gAssignedVoices[slot] == voiceCount)) {
        buttonBackgroundColour = (tRgb)RGB_BACKGROUND_GREY;
    } else {
        buttonBackgroundColour = (tRgb)RGB_RED_5;
    }
    snprintf(buff, sizeof(buff), "%u", voiceCount);
    gVoiceCountRectangle = draw_button(mainArea, {{248, 60}, {get_text_width("XX", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, buff, buttonBackgroundColour);

    module.key.slot      = slot;
    module.key.location  = locationMorph;
    module.key.index     = PATCH_VOLUME;

    if (read_module(module.key, &module) == true) {
        snprintf(buff, sizeof(buff), "%s", patchVolumeStrMap[module.param[variation][VOLUME_LEVEL].value]);
        render_text(mainArea, {{gPatchVolumeRectangle.coord.x, gPatchVolumeRectangle.coord.y - 12}, {NULL, STANDARD_TEXT_HEIGHT}}, buff);
        module.param[variation][VOLUME_LEVEL].rectangle = render_dial(mainArea, gPatchVolumeRectangle, module.param[variation][VOLUME_LEVEL].value, 127, 0, RGB_GREY_7);
        write_module(module.key, &module);
    }

    for (int i = 0; i < array_size_main_button_array(); i++) {
        rectangle                     = {gMainButtonArray[i].coord, {get_text_width(gMainButtonArray[i].text, STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}};

        switch (gMainButtonArray[i].anchor) {
            case anchorTopRight:
            {
                rectangle.coord.x = (get_render_width() / gGlobalGuiScale) + gMainButtonArray[i].coord.x;
                break;
            }
            default:
            {
                break;
            }
        }

        if (gMainButtonArray[i].isPressed) {
            buttonBackgroundColour = (tRgb)RGB_GREY_7;
        } else {
            buttonBackgroundColour = gMainButtonArray[i].backgroundColour;
        }
        gMainButtonArray[i].rectangle = draw_button(mainArea, rectangle, gMainButtonArray[i].text, buttonBackgroundColour);
    }

    commsStateColour = RGB_BACKGROUND_GREY;

    switch (commsState) {
        case eCommsOnLine:
            commsStateText   = "Online";
            commsStateColour = RGB_GREEN_7;
            break;
        default:
            commsStateText   = "Offline";
            break;
    }
    set_rgb_colour(commsStateColour);
    rectangle        = {{220, 8}, {get_text_width("Offline", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}};
    draw_button(mainArea, rectangle, commsStateText, commsStateColour);

    // Cable colour visibility toggles — 6 small squares
    //uint32_t hiddenMask = atomic_load(&gHiddenCableMask);

    for (int i = 0; i < cableColourMax; i++) {
        bool   visible = gPatchDescr[slot].visible[i];
        tRgb   colour  = gCableColourMap[i];
        double x       = 700.0 + (i * (get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT) + 5));

        if (visible) {
            gCableColourToggleRect[i] = draw_button(mainArea, {{x, 40.0}, {get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, "X", colour);
        } else {
            gCableColourToggleRect[i] = draw_button(mainArea, {{x, 40.0}, {get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, " ", colour);
        }
    }

    for (int i = 0; i < cableColourMax; i++) {
        tRgb   colour = gCableColourMap[i];
        double x      = 700.0 + (i * (get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT) + 5));

        if (i == gCableColour) {
            gCableColourSelectRect[i] = draw_button(mainArea, {{x, 60}, {get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, "X", colour);
        } else {
            gCableColourSelectRect[i] = draw_button(mainArea, {{x, 60}, {get_text_width("X", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, " ", colour);
        }
    }

    //bool hideAll = atomic_load(&gCablesHideAll);
    bool transp = atomic_load(&gCablesTransparent);

    gHideAllCablesRect     = draw_button(mainArea,
                                         {{700, 20}, {get_text_width("Hide", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}},
                                         "Hide", (tRgb)RGB_BACKGROUND_GREY);

    gTransparentCablesRect = draw_button(mainArea,
                                         {{740, 20}, {get_text_width("Hide", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}},
                                         "Dim", transp ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);

    if (atomic_load(&gMasterClockRunning)) {
        buttonBackgroundColour = (tRgb)RGB_GREEN_ON;
    } else {
        buttonBackgroundColour = (tRgb)RGB_BACKGROUND_GREY;
    }
    snprintf(buff, sizeof(buff), "%u BPM", atomic_load(&gMasterClock));
    draw_button(mainArea,
                {{500, 8}, {get_text_width("XXX BPM", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}},
                buff, buttonBackgroundColour);

    if (atomic_load(&gPerfMode)) {
        snprintf(buff, sizeof(buff), "Perf Mode");
    } else {
        snprintf(buff, sizeof(buff), "Patch Mode");
    }
    draw_button(mainArea,
                {{20, 42}, {get_text_width("Patch Mode", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}},
                buff, (tRgb)RGB_BACKGROUND_GREY);

    snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].cycles[locationVa]);
    draw_button(mainArea,
                {{600, 26}, {get_text_width("XX.X%", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}},
                buff, buttonBackgroundColour);
    snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].cycles[locationFx]);
    draw_button(mainArea,
                {{600, 42}, {get_text_width("XX.X%", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}},
                buff, buttonBackgroundColour);
    snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].mem[locationVa]);
    draw_button(mainArea,
                {{644, 26}, {get_text_width("XX.X%", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}},
                buff, buttonBackgroundColour);
    snprintf(buff, sizeof(buff), "%.1f%%", gResourceAlloc[slot].mem[locationFx]);
    draw_button(mainArea,
                {{644, 42}, {get_text_width("XX.X%", STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}},
                buff, buttonBackgroundColour);
}

void wake_glfw(void) {
    atomic_store(&gReDraw, true);

    // Safe GLFW call from any thread
    glfwPostEmptyEvent();
}

void notify_full_patch_change(void) {
    atomic_store(&gLocation, locationVa);
    gMainButtonArray[vaButtonId].backgroundColour = (tRgb)RGB_GREEN_ON;
    gMainButtonArray[fxButtonId].backgroundColour = (tRgb)RGB_BACKGROUND_GREY;
    // Set scrollbars back to top/left
    gScrollState.xBar                             = (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN;
    set_x_scroll_bar(gScrollState.xBar);
    gScrollState.yBar                             = (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN;
    set_y_scroll_bar(gScrollState.yBar);
}

void setup_render_context(void) {
    glDisable(GL_DEPTH_TEST);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, get_render_width(), get_render_height(), 0, -1, 1);
    glMatrixMode(GL_MODELVIEW);
}

void init_graphics(void) {
    int           fbWidth           = 0;
    int           fbHeight          = 0;
    int           windowWidth       = 0;
    int           windowHeight      = 0;
    GLFWmonitor * monitor           = NULL;
    float         xScale            = 1;
    float         yScale            = 1;
    char          title[128]        = {0};

    snprintf(title, sizeof(title), "%s - Build %s %s", WINDOW_TITLE, __DATE__, __TIME__);

    glfwSetErrorCallback(error_callback);

    if (!glfwInit()) {
        exit(EXIT_FAILURE);
    }
    register_glfw_wake_cb(wake_glfw);
    register_full_patch_change_notify_cb(notify_full_patch_change);

    monitor      = glfwGetPrimaryMonitor();

    int           x, y, width, height;
    double        widthScaleFactor  = 1;
    double        heightScaleFactor = 1;
    double        scaleFactor       = 1;
    glfwGetMonitorWorkarea(monitor, &x, &y, &width, &height);
    glfwGetMonitorContentScale(monitor, &xScale, &yScale);
    windowWidth  = TARGET_FRAME_BUFF_WIDTH / xScale;
    windowHeight = TARGET_FRAME_BUFF_HEIGHT / yScale;

    if ((windowWidth > width) || (windowHeight > height)) {
        heightScaleFactor = (double)windowHeight / (double)height;
        widthScaleFactor  = (double)windowWidth / (double)width;

        if (heightScaleFactor >= widthScaleFactor) {
            scaleFactor = heightScaleFactor;
        } else {
            scaleFactor = widthScaleFactor;
        }
        windowWidth       = (int)((double)windowWidth / scaleFactor);
        windowHeight      = (int)((double)windowHeight / scaleFactor);
        gGlobalGuiScale   = gGlobalGuiScale / scaleFactor;
    }
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_COCOA_GRAPHICS_SWITCHING, GLFW_TRUE);
    gWindow      = glfwCreateWindow(windowWidth, windowHeight, title, NULL, NULL);

    if (!gWindow) {
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    glfwSetWindowSizeLimits(gWindow, windowWidth / gGlobalGuiScale, windowHeight / gGlobalGuiScale, GLFW_DONT_CARE, GLFW_DONT_CARE);
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

void set_patch_name_from_filename(uint32_t slot, const char * filepath) {
    const char * base                            = filepath;
    const char * p                               = filepath;
    char         patchName[CLAVIA_NAME_SIZE + 1] = {0};
    int          i                               = 0;

    // Find last path separator
    while (*p != '\0') {
        if (*p == '/' || *p == '\\') {
            base = p + 1;
        }
        p++;
    }
    // Copy up to PATCH_NAME_SIZE chars, stop at '.' (extension)
    memset(patchName, 0, sizeof(patchName));

    while (i < CLAVIA_NAME_SIZE && base[i] != '\0' && base[i] != '.') {
        patchName[i] = base[i];
        i++;
    }
    patch_name_set(slot, patchName);

    LOG_DEBUG("Patch name from file: '%s'\n", patchName);
}

void read_file_into_memory_and_process(const char * filepath) {
    int64_t   byteOffset = 0;
    int64_t   fileSize   = 0;
    FILE *    file       = NULL;
    uint8_t * buff       = NULL;
    size_t    readSize   = 0;
    uint8_t   version    = 23;
    uint8_t   type       = 0;
    uint32_t  readCrc    = 0;
    uint32_t  calcCrc    = 0;
    uint32_t  slot       = atomic_load(&gSlot);

    file     = fopen(filepath, "rb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        return;
    }
    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    clearerr(file);

    buff     = (uint8_t *)malloc(fileSize);

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

    readCrc  = buff[fileSize - 2] << 8 | buff[fileSize - 1];
    calcCrc  = calc_crc16(buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));

    if (readCrc == calcCrc) {
        version                = buff[byteOffset++];
        type                   = buff[byteOffset++];
        LOG_DEBUG("Version %u\n", version);
        LOG_DEBUG("Type %u\n", type);

        /* TODO - implement clear down commands as an init/clear slot function? */
        database_delete_cables_by_slot(slot);
        database_delete_modules_by_slot(slot);
        gMorphCount[slot]      = 0;
        gNote2Size[slot]       = 0;
        gControllerCount[slot] = 0;
        gPatchNotesSize[slot]  = 0;
        memset(&(gPatchDescr[slot]), 0, sizeof(gPatchDescr[0]));
        memset(&(gKnobArray[slot]), 0, sizeof(gKnobArray[0]));
        memset(gNote2[slot], 0, sizeof(gNote2[0]));
        memset(&(gControllerArray[slot]), 0, sizeof(gControllerArray[0]));
        memset(gPatchNotes[slot], 0, sizeof(gPatchNotes[0]));

        if (type == 0) {
            parse_patch(slot, buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));  // TODO: parse_patch should really be in a commonly accessible source file, for file or USB access
        } // 1 = performance

        set_patch_name_from_filename(slot, filepath);

        // If online, push to device immediately
        if (atomic_load(&gCommsState) == eCommsOnLine) {
            tMessageContent msg = {0};
            msg.cmd  = eMsgCmdWritePatch;
            msg.slot = slot;
            msg_send(&gCommandQueue, &msg);
        }
    } else {
        LOG_WARNING("CRC check failed\n");
    }
    free(buff);
    fclose(file);
}

void write_database_to_file(const char * filepath) {
    FILE *    file           = NULL;
    //uint8_t ch          = 0;
    size_t    writtenSize    = 0;
    char      charBuff[1024] = {0};
    char      eol[]          = {0x0d, 0x0a, 0x00};
    uint8_t * buff           = NULL;
    uint32_t  bitPos         = 0;
    uint32_t  calcCrc        = 0;
    uint32_t  slot           = atomic_load(&gSlot);

    file = fopen(filepath, "wb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        return;
    }
    // Couldn't really find a nice way to construct the write buffer, so allocating on the heap
    buff = (uint8_t *)malloc(PATCH_FILE_SIZE);

    if (buff == NULL) {
        LOG_ERROR("Failed to allocate buffer\n");
        fclose(file);
        return;
    }
    memset(buff, 0, PATCH_FILE_SIZE);

    // Header text, which seems to be constant across latest patch files
    snprintf(charBuff, sizeof(charBuff) - 1, "Version=Nord Modular G2 File Format 1");
    fwrite(charBuff, 1, strlen(charBuff), file);
    fwrite(eol, 1, strlen(eol), file);
    snprintf(charBuff, sizeof(charBuff) - 1, "Type=Patch");
    fwrite(charBuff, 1, strlen(charBuff), file);
    fwrite(eol, 1, strlen(eol), file);
    snprintf(charBuff, sizeof(charBuff) - 1, "Version=23");
    fwrite(charBuff, 1, strlen(charBuff), file);
    fwrite(eol, 1, strlen(eol), file);
    snprintf(charBuff, sizeof(charBuff) - 1, "Info=BUILD 320");
    fwrite(charBuff, 1, strlen(charBuff), file);
    fwrite(eol, 1, strlen(eol), file);
    charBuff[0] = '\0';
    fwrite(charBuff, 1, 1, file);

    write_bit_stream(buff, &bitPos, 8, 23); // Version
    write_bit_stream(buff, &bitPos, 8, 0);  // Type (0 = patch, 1 = performance when we get round to implementing that)

    write_patch_descr(slot, buff, &bitPos);
    write_module_list(slot, locationVa, buff, &bitPos);
    write_module_list(slot, locationFx, buff, &bitPos);
    write_current_note_2(slot, buff, &bitPos);
    write_cable_list(slot, locationVa, buff, &bitPos);
    write_cable_list(slot, locationFx, buff, &bitPos);
    write_param_list(slot, locationMorph, buff, &bitPos, NUM_VARIATIONS);
    write_param_list(slot, locationVa, buff, &bitPos, NUM_VARIATIONS);
    write_param_list(slot, locationFx, buff, &bitPos, NUM_VARIATIONS);
    write_morph_params(slot, buff, &bitPos, NUM_VARIATIONS);
    write_knobs(slot, buff, &bitPos);
    write_controllers(slot, buff, &bitPos);
    write_param_names(slot, locationMorph, buff, &bitPos);
    write_param_names(slot, locationVa, buff, &bitPos);
    write_param_names(slot, locationFx, buff, &bitPos);
    write_module_names(slot, locationVa, buff, &bitPos);
    write_module_names(slot, locationFx, buff, &bitPos);
    write_patch_notes(slot, buff, &bitPos);

    bitPos      = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(bitPos)); // Final byte alignment round-up

    calcCrc     = calc_crc16(buff, BIT_TO_BYTE_ROUND_UP(bitPos));

    write_bit_stream(buff, &bitPos, 16, calcCrc);

    writtenSize = fwrite(buff, 1, BIT_TO_BYTE_ROUND_UP(bitPos), file);

    if (writtenSize != BIT_TO_BYTE_ROUND_UP(bitPos)) {
        LOG_ERROR("Written %zu of %u\n", writtenSize, BIT_TO_BYTE_ROUND_UP(bitPos));
    }

    if (BIT_TO_BYTE_ROUND_UP(bitPos) > ((PATCH_FILE_SIZE * 3) / 4)) {
        LOG_ERROR("Write file size > 3/4 of %d, might need to increase PATCH_FILE_SIZE\n", PATCH_FILE_SIZE);
    }
    free(buff);
    fclose(file);
}

static void on_file_opened(const char * path) {
    if (path) {
        LOG_INFO("Selected file: %s", path);
        read_file_into_memory_and_process(path);
        //set_window_title(path);
    }
    atomic_store(&gNeedFocus, true);
    wake_glfw();
}

static void on_file_saved(const char * path) {
    uint32_t slot = atomic_load(&gSlot);

    if (path) {
        LOG_INFO("Saving file: %s", path);
        write_database_to_file(path);
        set_patch_name_from_filename(slot, path);
        //set_window_title(path);
    }
    atomic_store(&gNeedFocus, true);
    wake_glfw();
}

static void check_action_flags(void) {
    uint32_t slot                              = atomic_load(&gSlot);
    char     patchName[CLAVIA_NAME_SIZE + 1]   = {0};
    char     defaultName[CLAVIA_NAME_SIZE + 6] = {0}; // name + ".pch2\0"

    if (gShowOpenFileReadDialogue) {
        gShowOpenFileReadDialogue = false;
        open_file_read_dialogue_async(on_file_opened);
    }

    if (gShowOpenFileWriteDialogue) {
        gShowOpenFileWriteDialogue = false;

        patch_name_get(slot, patchName, sizeof(patchName));

        if (patchName[0] != '\0') {
            snprintf(defaultName, sizeof(defaultName), "%s.pch2", patchName);
        } else {
            strncpy(defaultName, "patch.pch2", sizeof(defaultName) - 1);
        }
        open_file_write_dialogue_async(on_file_saved, defaultName);
    }

    if (atomic_load(&gNeedFocus)) {
        atomic_store(&gNeedFocus, false);
        glfwFocusWindow(gWindow);
    }
}

void do_graphics_loop(void) {
    bool reDraw = false;

    while ((gQuitAll == false) && (!glfwWindowShouldClose(gWindow))) {
        check_action_flags();

        reDraw = atomic_load(&gReDraw);
        atomic_store(&gReDraw, false);

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
            glfwWaitEvents();
        }
    }
}

void clean_up_graphics(void) {
    // Clean up
    FT_Done_Face(gFace);
    FT_Done_FreeType(gLibrary);
    free_textures();

    // glfwMakeContextCurrent(NULL); // TODO: Probably not needed, leaving whilst more tests are done
    glfwDestroyWindow(gWindow);
    gWindow = NULL;
    glfwTerminate();
}

#ifdef __cplusplus
}
#endif
