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

#include <ft2build.h>
#include FT_FREETYPE_H
#pragma clang diagnostic pop

#include <math.h>
#include <unistd.h>

// stb_image_write is already bundled as a GLFW build dependency — reused here
// (rather than a second PNG library) purely for the backdoor SCREENSHOT command.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../SynthLib/ThirdParty/glfw/deps/stb_image_write.h"
#pragma clang diagnostic pop

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "utils.h"
#include "msgQueue.h"
#include "protocol.h"
#include "usbComms.h"
#include "graphics.h"
#include "topbarRender.h"
#include "splitView.h"
#include "utilsGraphics.h"
#include "synthlibWindow.h"
#include "synthlibPopups.h"
#include "mouseHandle.h"
#include "dataBase.h"
#include "moduleGraphics.h"
#include "alertDialog.h"
#include "moduleResourcesAccess.h"
#include "topbarResourcesAccess.h"
#include "globalVars.h"
#include "synthSettingsResources.h"
#include "misc.h"
#include "clickRegion.h"
#include "patchParamsResources.h"
#include "perfSettingsResources.h"
#include "menus.h"
#include "undo.h"
#include "deviceSync.h"
#include "mutatorUI.h"
#include "mousePanels.h"
#include "paramPages.h"
#include "paramOverview.h"
#include "midiCcList.h"
#include "floatingPanel.h"
#include "helpPanel.h"
#include "virtualKeyboard.h"
#include "patchAdjuster.h"
#include "soundEngine.h"
#include "paramOverlay.h"
#include <strings.h>

#include "appMenuBar.h"
#include "selection.h"
#include "contextMenu.h"
#include "fileBrowser.h"
#include "bankBrowser.h"
#include "alertDialog.h"
#include "synthlibHost.h"
#include "synthlibScale.h"
#include "synthlibPersistence.h"

static void register_app_popups(void);

static FT_Library      gLibrary        = {0};
static FT_Face         gFace           = {0};
static _Atomic bool    gNeedFocus      = false;

#define MAX_NOTE_VISUAL_LINES    1000

typedef struct {
    int  bufStart;
    int  bufEnd;
    bool hardBreak;
} tNoteVisualLine;

static tNoteVisualLine gNoteLines[MAX_NOTE_VISUAL_LINES];
static int             gNoteLineCount  = 0;
static int             gNoteScrollLine = 0;
static double          gNoteTextX      = 0.0;
static double          gNoteTextY0     = 0.0;
static double          gNoteLineH      = 0.0;
static double          gNoteTextW      = 0.0;
static double          gNoteTextHParam = 0.0;

static int find_wrap_point(const char * text, int textLen, double textW, double textH) {
    if (textLen <= 0) {
        return 0;
    }
    char tmp[PATCH_NOTES_SIZE + 1];
    strncpy(tmp, text, (size_t)textLen);
    tmp[textLen] = '\0';

    if (get_text_width(tmp, textH, eNoCache) <= textW) {
        return textLen;
    }
    int  lo = 1, hi = textLen;

    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;

        strncpy(tmp, text, (size_t)mid);
        tmp[mid] = '\0';

        if (get_text_width(tmp, textH, eNoCache) <= textW) {
            lo = mid;
        } else{
            hi = mid - 1;
        }
    }
    int  charBreak = lo;

    int  wordBreak = charBreak;

    while (wordBreak > 0 && text[wordBreak - 1] != ' ') {
        wordBreak--;
    }
    return (wordBreak > 0) ? wordBreak : charBreak;
}

static void build_note_visual_lines(const char * buf, double textW, double textH) {
    gNoteLineCount = 0;
    int len = (int)strlen(buf);
    int pos = 0;

    while (gNoteLineCount < MAX_NOTE_VISUAL_LINES) {
        int logicalEnd = pos;

        while (logicalEnd < len && buf[logicalEnd] != '\r') {
            logicalEnd++;
        }
        int segStart   = pos;

        while (gNoteLineCount < MAX_NOTE_VISUAL_LINES) {
            int  remaining = logicalEnd - segStart;

            if (remaining <= 0) {
                if (segStart == pos) {
                    gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
                        segStart, segStart, true
                    };
                }
                break;
            }
            int  wrapAt    = find_wrap_point(buf + segStart, remaining, textW, textH);
            bool softWrap  = (wrapAt < remaining);
            gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
                segStart, segStart + wrapAt, !softWrap
            };
            segStart                    += wrapAt;

            if (!softWrap) {
                break;
            }
        }

        if (logicalEnd >= len) {
            break;
        }
        pos = logicalEnd + 1;

        if (pos >= len && gNoteLineCount < MAX_NOTE_VISUAL_LINES) {
            gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
                len, len, true
            };
            break;
        }
    }

    if (gNoteLineCount == 0) {
        gNoteLines[gNoteLineCount++] = (tNoteVisualLine){
            0, 0, true
        };
    }
}

static int find_note_cursor_line(int cursorPos) {
    int result = 0;

    for (int i = 0; i < gNoteLineCount; i++) {
        if (gNoteLines[i].bufStart <= cursorPos) {
            result = i;
        }
    }

    return result;
}

void resize_window(int w, int h) {
    glfwSetWindowSize((GLFWwindow *)synthlib_window(), w, h);
}

void reposition_window(int x, int y) {
    glfwSetWindowPos((GLFWwindow *)synthlib_window(), x, y);
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
    glfwSetWindowTitle((GLFWwindow *)synthlib_window(), newTitle);
}

// NO GLFWwindow * ARGUMENT ANY MORE, AND THERE NEVER SHOULD HAVE BEEN ONE. It took a window and read
// nothing from it: the sizes come from get_render_width()/get_render_height() and the bars from the
// split view. Every OTHER function here that takes a GLFWwindow * is a GLFW callback, whose signature
// GLFW dictates — this was the only one advertising a dependency it did not have, which is
// vst3/plugin-gui-notes.md's third observation. Worth removing rather than shrugging at: a signature
// like that is what made the earlier extractions look daunting when they turned out to be mechanical,
// and the plug-in already draws its own bars through render_pane_scrollbars().
void render_scrollbars(void) {
    double renderWidth  = get_render_width() / gGlobalGuiScale;
    double renderHeight = get_render_height() / gGlobalGuiScale;

    // The tracks and thumbs belong to the split view now — one vertical bar per pane and a
    // horizontal one for the focused pane, all with proportional thumbs. The filler square that used
    // to sit where the two bars met has gone with them: both bars now stop the same distance clear
    // of the corner, so there is no junction left to cover.
    (void)renderWidth;
    (void)renderHeight;

    gScrollState.xThumb = (tRectangle){
        0
    };
    gScrollState.yThumb = (tRectangle){
        0
    };
    render_pane_scrollbars();
}

void wake_glfw(void) {
    // synthlib_request_redraw() already does both of these (safe to call from any thread).
    synthlib_request_redraw();
}

void notify_full_patch_change(void) {
    gLocation         = locationVa;
    // Set scrollbars back to top/left
    gScrollState.xBar = (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN;
    set_x_scroll_bar(gScrollState.xBar);
    gScrollState.yBar = (SCROLLBAR_LENGTH / 2.0) + SCROLLBAR_MARGIN;
    set_y_scroll_bar(gScrollState.yBar);
}

void init_graphics(void) {
    char              title[128]           = {0};

    snprintf(title, sizeof(title), "%s - Build %s %s", WINDOW_TITLE, __DATE__, __TIME__);

    // Things that must be in place before the first frame but need no window. They stay here rather
    // than moving into SynthLib because every one of them is this application's own business: what
    // its patch categories are, that it opens with a single pane, what a wake-up from the USB thread
    // should do.
    //
    // The bank browser's Category mode sorts its groups by name, which buries the two categories the
    // player actually assigns themselves at the bottom under U. Pin them to the top instead. Read
    // out of patchTypeStrMap rather than spelt again here, so renaming a category cannot silently
    // unpin it. Set once: it holds for every browser this app opens, all of which use this same map.
    const char *const priorityCategories[] = {
        patchTypeStrMap[patchTypeUser1], patchTypeStrMap[patchTypeUser2]
    };

    bank_browser_set_priority_categories(priorityCategories, ARRAY_SIZE(priorityCategories));

    split_view_init();   // one pane showing the Voice Area — the pre-split behaviour, as the default
    register_glfw_wake_cb(wake_glfw);
    register_full_patch_change_notify_cb(notify_full_patch_change);
    topbar_init_controls();

    // THE WINDOW, ITS SCALE AND ITS INPUT WIRING ARE SYNTHLIB'S NOW — see synthlibWindow.h. What
    // used to be ~60 lines here, and the same ~60 lines in SynthEdit and EmuUtility, is a config and
    // a callback table. The six callbacks that only ever called back into SynthLib (error,
    // framebuffer size, content scale, window size, window position, window close) went with it;
    // the ones below are the ones that reach into this app's own domain.
    //
    // The dial mode is set through the config rather than left to default, because this app is the
    // odd one out: SynthLib defaults to eDialModeVertical to match EmuUtility/SynthEdit, and this
    // one wants rotary. It is applied before setup_main_menu()'s load_saved_settings() (misc.mm)
    // runs, so a real saved value still wins.
    // The popup coordinator needs to know this app's own panels and its menu bar before the first
    // frame — see synthlibPopups.h. Through a function because the table names render functions
    // defined further down this file.
    register_app_popups();

    synthlib_window_create(&(tSynthLibWindowConfig){
        .title        = title,
        .targetWidth  = TARGET_FRAME_BUFF_WIDTH,
        .targetHeight = TARGET_FRAME_BUFF_HEIGHT,
        .dialMode     = eDialModeRotary,
        .theme        = (tSynthLibTheme){
            .topBarHeight   = TOP_BAR_HEIGHT + MENU_BAR_HEIGHT,
            .orange1        = (tRgb)RGB_ORANGE_1,
            .orange2        = (tRgb)RGB_ORANGE_2,
            .greenOn        = (tRgb)RGB_GREEN_ON,
            .backgroundGrey = (tRgb)RGB_BACKGROUND_GREY,
        },
        .mouseCoord   = get_global_gui_scaled_mouse_coord,
        .handlers     = &(const tSynthLibInputHandlers){
            .mouseButton = mouse_button,
            .cursorPos   = cursor_pos,
            .key         = key_callback,
            .character   = char_event,
            .scroll      = scroll_event,
            .windowFocus = window_focus_callback,   // clears held modifiers — see inputState.h
        },
    }, NULL);

    FT_Init_FreeType(&gLibrary);
    FT_New_Face(gLibrary, "/System/Library/Fonts/Supplemental/Arial.ttf", 0, &gFace);
    FT_Set_Char_Size(gFace, 0, 48 * 64, 300, 300);

    // Preload glyph textures
    if (!preload_glyph_textures("/System/Library/Fonts/Supplemental/Arial.ttf", 72.0f)) {
        LOG_ERROR("Failed to preload glyph textures\n");
    }
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
    uint32_t  slot       = gSlot;

    if (gCommsState == eCommsOnLine) {
        // Online: hand the entire load to the USB thread as one ordered command. It opens the file,
        // validates the CRC, sniffs the type byte (patch vs perf) and does the clear/parse/push
        // itself, so the UI thread touches neither the file nor the shared DB. See load_file_to_device().
        tMessageContent msg = {0};
        msg.cmd                = eMsgCmdLoadFile;
        msg.patchFileData.slot = slot;
        COPY_STRING(msg.patchFileData.filePath, filepath);
        msg_send(&gToUsbThread, &msg);
        device_op_begin("Loading...");
        return;
    }
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
        version = buff[byteOffset++];
        type    = buff[byteOffset++];
        LOG_DEBUG("Version %u\n", version);
        LOG_DEBUG("Type %u\n", type);

        // Offline only: the online case returned early above (handled on the USB thread). With no
        // device there's no mode switch and no patch-change cascade to race, so we parse straight
        // into the database on this thread.
        if (type == 0) {
            clear_slot_data(slot);
            parse_patch(slot, buff + byteOffset, (uint32_t)((fileSize - byteOffset) - 2));
            set_patch_name_from_filename(slot, filepath);
        } else if (type == 1) {
            // Performance file — parse_perf clears all 4 slots and populates them; slot names come
            // from the file itself so set_patch_name_from_filename is not called. Derive the
            // performance name from the filename (strip dir + .prf2).
            for (int i = 0; i < MAX_SLOTS; i++) {
                clear_slot_data(i);
            }

            const char * slash    = strrchr(filepath, '/');
            const char * baseName = slash ? slash + 1 : filepath;
            COPY_STRING(gGlobalSettings.perfName, baseName);
            char *       dot      = strrchr(gGlobalSettings.perfName, '.');

            if (dot) {
                *dot = '\0';
            }
            gGlobalSettings.perfMode = 1;
            parse_perf(buff + byteOffset, (int)((fileSize - byteOffset) - 2));
        }
    } else {
        LOG_WARNING("CRC check failed\n");
    }
    free(buff);
    fclose(file);
}

int write_database_to_file(const char * filepath, uint32_t slot) {
    FILE *    file           = NULL;
    //uint8_t ch          = 0;
    size_t    writtenSize    = 0;
    char      charBuff[1024] = {0};
    char      eol[]          = {0x0d, 0x0a, 0x00};
    uint8_t * buff           = NULL;
    uint32_t  bitPos         = 0;
    uint32_t  calcCrc        = 0;

    file = fopen(filepath, "wb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        return EXIT_FAILURE;
    }
    // Couldn't really find a nice way to construct the write buffer, so allocating on the heap
    buff = (uint8_t *)malloc(PATCH_FILE_SIZE);

    if (buff == NULL) {
        LOG_ERROR("Failed to allocate buffer\n");
        fclose(file);
        return EXIT_FAILURE;
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
    // No Morph param-names section — matches the reference structure; see the matching comment in
    // push_slot_to_device() (usbComms.c).
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
    return EXIT_SUCCESS;
}

int write_perf_to_file(const char * filepath) {
    FILE *    file        = NULL;
    size_t    writtenSize = 0;
    char      eol[]       = {0x0d, 0x0a, 0x00};
    char      nullByte    = '\0';
    uint8_t * buff        = NULL;
    uint32_t  bitPos      = 0;
    uint32_t  calcCrc     = 0;

    file = fopen(filepath, "wb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        return EXIT_FAILURE;
    }
    buff = (uint8_t *)malloc(PERF_FILE_SIZE);

    if (buff == NULL) {
        LOG_ERROR("Memory allocation failed\n");
        fclose(file);
        return EXIT_FAILURE;
    }
    memset(buff, 0, PERF_FILE_SIZE);

    fwrite("Version=Nord Modular G2 File Format 1", 1, 37, file);
    fwrite(eol, 1, 2, file);
    fwrite("Type=Performance", 1, 16, file);
    fwrite(eol, 1, 2, file);
    fwrite("Version=23", 1, 10, file);
    fwrite(eol, 1, 2, file);
    fwrite("Info=BUILD 320", 1, 14, file);
    fwrite(eol, 1, 2, file);
    fwrite(&nullByte, 1, 1, file);

    write_bit_stream(buff, &bitPos, 8, 23); // version
    write_bit_stream(buff, &bitPos, 8, 1);  // type = performance

    write_perf_header(buff, &bitPos);

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        uint32_t savedU1 = gPatchDescr[slot].unknown1;
        uint32_t savedU2 = gPatchDescr[slot].unknown2;
        gPatchDescr[slot].unknown1 = 0;
        gPatchDescr[slot].unknown2 = 0;
        write_patch_descr(slot, buff, &bitPos);
        gPatchDescr[slot].unknown1 = savedU1;
        gPatchDescr[slot].unknown2 = savedU2;
        write_module_list(slot, locationVa, buff, &bitPos);
        write_module_list(slot, locationFx, buff, &bitPos);
        write_current_note_2_perf(slot, buff, &bitPos);
        write_cable_list(slot, locationVa, buff, &bitPos);
        write_cable_list(slot, locationFx, buff, &bitPos);
        write_param_list(slot, locationMorph, buff, &bitPos, NUM_VARIATIONS);
        write_param_list(slot, locationVa, buff, &bitPos, NUM_VARIATIONS);
        write_param_list(slot, locationFx, buff, &bitPos, NUM_VARIATIONS);
        write_morph_params(slot, buff, &bitPos, NUM_VARIATIONS);
        write_knobs(slot, buff, &bitPos);
        write_controllers(slot, buff, &bitPos);
        // No Morph param-names section — matches the reference structure and write_patch_to_file();
        // see the matching comment in push_slot_to_device() (usbComms.c).
        write_param_names(slot, locationVa, buff, &bitPos);
        write_param_names(slot, locationFx, buff, &bitPos);
        write_module_names(slot, locationVa, buff, &bitPos);
        write_module_names(slot, locationFx, buff, &bitPos);
        write_slot_separator(buff, &bitPos); // 0x6f — same as PATCH_NOTES type, not written in perf
    }

    write_global_knobs(buff, &bitPos);

    bitPos      = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(bitPos));
    calcCrc     = calc_crc16(buff, BIT_TO_BYTE_ROUND_UP(bitPos));
    write_bit_stream(buff, &bitPos, 16, calcCrc);

    writtenSize = fwrite(buff, 1, BIT_TO_BYTE_ROUND_UP(bitPos), file);

    if (writtenSize != BIT_TO_BYTE_ROUND_UP(bitPos)) {
        LOG_ERROR("Written %zu of %u\n", writtenSize, BIT_TO_BYTE_ROUND_UP(bitPos));
    }

    if (BIT_TO_BYTE_ROUND_UP(bitPos) > ((PERF_FILE_SIZE * 3) / 4)) {
        LOG_ERROR("Write file size > 3/4 of %d, might need to increase PERF_FILE_SIZE\n", PERF_FILE_SIZE);
    }
    free(buff);
    fclose(file);
    return EXIT_SUCCESS;
}

// Remembers where the patch (or performance) currently on screen came from, so File > Save can
// write straight back to it. Called after an open and after a save — the same rule every editor
// uses: the last file you opened or saved to is the one Save overwrites. Recorded per slot,
// because each of the four can have come from a different file; perf files own all four at once
// and so get a single path of their own.
static void remember_file_path(const char * path) {
    // Read the slot ONCE. COPY_STRING expands its destination three times — the strncpy, the
    // sizeof, and the terminator write — and gSlot is atomic, so writing gSavedPatchPath[gSlot]
    // directly is three separate loads. A slot change part way through would terminate a different
    // buffer than the one just copied into.
    uint32_t slot = gSlot;

    if ((path == NULL) || (path[0] == '\0')) {
        return;
    }

    if (slot >= MAX_SLOTS) {
        LOG_ERROR("remember_file_path: slot %u out of range\n", slot);
        return;
    }

    // File > Save hands back the very buffer it is about to write into: the path being saved to IS
    // the remembered path (see eRspSaveToCurrentPath). strncpy's arguments are restrict-qualified,
    // so copying a buffer onto itself is undefined behaviour rather than a harmless no-op, and it
    // crashed here intermittently. Save As never hit it because the panel supplies a fresh buffer.
    if (gGlobalSettings.perfMode == 1) {
        if (gSavedPerfPath != path) {
            COPY_STRING(gSavedPerfPath, path);
        }
    } else if (gSavedPatchPath[slot] != path) {
        COPY_STRING(gSavedPatchPath[slot], path);
    }
}

static void on_file_opened(const char * path) {
    if (path) {
        LOG_INFO("Selected file: %s", path);
        read_file_into_memory_and_process(path);
        recent_files_add(path);    // File > Open Recent — same event that settles File > Save's target
        remember_file_path(path);  // Read AFTER the load: it is the load that settles perf vs patch
        //set_window_title(path);
    }
    gNeedFocus = true;
    wake_glfw();
}

// ── Offline-edit conflict on reconnect ──────────────────────────────────────
//
// The USB thread has found edits made while the G2 was away and parked itself until the user
// decides whose copy wins. Recovery files are already on disk by the time the dialog appears, so
// every answer here — including Escape — is safe.
//
// Non-zero only between choosing "Save As..." and that save finishing, so on_file_saved() knows to
// finish resolving the conflict afterwards. The dialog is modal and the USB thread is parked, so
// no second conflict can start meanwhile.
static uint32_t sConflictMaskPendingSave = 0;

// Tells the USB thread to resume: push the editor's slots to the G2, or take the G2's patches.
// Either way the undo history goes, because after a wholesale resend (or a wholesale replacement)
// its entries describe a device state that no longer exists.
static void resolve_offline_conflict(uint32_t slotMask, bool pushToDevice) {
    tMessageContent msg = {0};

    msg.cmd                          = eMsgCmdResolveOfflineEdits;
    msg.offlineEditData.slotMask     = slotMask;
    msg.offlineEditData.pushToDevice = pushToDevice;
    msg_send(&gToUsbThread, &msg);

    undo_clear();
    wake_glfw();
}

static void on_offline_conflict_choice(int choice) {
    uint32_t slotMask = sConflictMaskPendingSave;

    sConflictMaskPendingSave = 0;

    switch (choice) {
        case 0:  // Send to Synth — the editor's patches win
            resolve_offline_conflict(slotMask, true);
            break;

        case 1:  // Save As... — park the resolve until the save has been through the browser
            sConflictMaskPendingSave = slotMask;
            file_menu_save_patch();
            break;

        default:  // Pull from Synth, and what Escape means: the G2's patches win
            resolve_offline_conflict(slotMask, false);
            break;
    }
}

void show_offline_conflict_dialog(uint32_t slotMask) {
    char     location[FILE_PATH_SIZE] = {0};
    char     slots[32]                = {0};
    char     message[512]             = {0};
    uint32_t recovered                = device_sync_write_recovery_files(slotMask, location, sizeof(location));
    size_t   used                     = 0;

    for (uint32_t slot = 0; slot < MAX_SLOTS; slot++) {
        if ((slotMask & (1u << slot)) != 0) {
            used += (size_t)snprintf(slots + used, sizeof(slots) - used, "%s%c",
                                     (used > 0) ? ", " : "", (char)('A' + slot));
        }
    }

    snprintf(message, sizeof(message),
             "You edited slot %s while the G2 was disconnected, so the editor and the synth now "
             "disagree.\n\n"
             "%s\n\n"
             "Send to Synth writes the editor's patches over the synth's. Pull from Synth replaces "
             "the editor's with the synth's. Either way the undo history is cleared.",
             slots,
             (recovered > 0)
             ? "Your edits have already been saved to a recovery file, so nothing here can lose them:"
             : "WARNING: a recovery file could not be written, so choosing Pull WILL discard your edits.");

    if ((recovered > 0) && (location[0] != '\0')) {
        used = strlen(message);
        snprintf(message + used, sizeof(message) - used, "\n%s", location);
    }
    sConflictMaskPendingSave = slotMask;  // Carried into the callback, which has no user-data slot
    show_choice("Offline Edits", message, "Send to Synth", "Save As...", "Pull from Synth",
                on_offline_conflict_choice);
}

static void on_file_saved(const char * path) {
    uint32_t slot = gSlot;

    if (path) {
        LOG_INFO("Saving file: %s", path);

        if (gGlobalSettings.perfMode == 1) {
            if (gCommsState == eCommsOnLine) {
                // Online: serialise the whole DB (all 4 slots) on the USB thread — same reason as the
                // patch save below: the DB read must be atomic against this thread's async reparses.
                tMessageContent msg = {0};
                msg.cmd = eMsgCmdSavePerfFile;
                COPY_STRING(msg.patchFileData.filePath, path);
                msg_send(&gToUsbThread, &msg);
                device_op_begin("Saving...");
            } else {
                write_perf_to_file(path);
            }
        } else if (gCommsState == eCommsOnLine) {
            // Online: serialise the slot on the USB thread so the DB read can't tear against the USB
            // thread's own DB writes (e.g. an async patch-change reparse). Name update (a single
            // field, harmless) stays here so the title bar reflects the saved filename immediately.
            tMessageContent msg = {0};
            msg.cmd                = eMsgCmdSavePatchFile;
            msg.patchFileData.slot = slot;
            COPY_STRING(msg.patchFileData.filePath, path);
            msg_send(&gToUsbThread, &msg);
            device_op_begin("Saving...");
            set_patch_name_from_filename(slot, path);
        } else {
            write_database_to_file(path, slot);
            set_patch_name_from_filename(slot, path);
        }
        remember_file_path(path);
    }

    // The save that a conflict's "Save As..." branch was waiting on. Resolve either way: a
    // cancelled save (path == NULL) still has to release the parked USB thread, and the recovery
    // file written before the dialog means taking the synth's copy is not a loss even then.
    if (sConflictMaskPendingSave != 0) {
        uint32_t slotMask = sConflictMaskPendingSave;

        sConflictMaskPendingSave = 0;

        if (path != NULL) {
            // Saved where they chose, so the automatic copy is redundant. Only here — on the Pull
            // branch that file is the only copy of the work that exists.
            device_sync_discard_recovery_files();
        }
        resolve_offline_conflict(slotMask, false);
    }
    gNeedFocus = true;
    wake_glfw();
}

// Fires after the user has seen the overwrite warning built from a gStorePeekComplete result
// (below) and clicked "Store...". The target is whatever peek_store_target() just recorded in
// gStorePeekBank/gStorePeekLocation — no separate captured-context state needed here since nothing
// can change those globals between the peek landing and this callback firing (both happen on the
// main thread, and the user can't trigger a second Store attempt while this alert is up).
static void on_store_confirmed(bool confirmed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdStorePatch;
    msg.bankLocationPerfData.bank     = gStorePeekBank;
    msg.bankLocationPerfData.location = gStorePeekLocation;
    msg.bankLocationPerfData.isPerf   = gStorePeekIsPerf;
    msg_send(&gToUsbThread, &msg);
}

// Same shape as on_store_confirmed above, but for Delete — target comes from
// gDeletePeekBank/gDeletePeekLocation/gDeletePeekIsPerf, set by peek_delete_target().
static void on_delete_confirmed(bool confirmed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdDeleteBankLocation;
    msg.bankLocationPerfData.bank     = gDeletePeekBank;
    msg.bankLocationPerfData.location = gDeletePeekLocation;
    msg.bankLocationPerfData.isPerf   = gDeletePeekIsPerf;
    msg_send(&gToUsbThread, &msg);
}

// Same shape as on_store_confirmed/on_delete_confirmed above, but for Load — target comes from
// gLoadPeekBank/gLoadPeekLocation/gLoadPeekIsPerf, set by peek_load_target().
static void on_load_confirmed(bool confirmed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd                           = eMsgCmdLoadPatch;
    msg.bankLocationPerfData.bank     = gLoadPeekBank;
    msg.bankLocationPerfData.location = gLoadPeekLocation;
    msg.bankLocationPerfData.isPerf   = gLoadPeekIsPerf;
    msg_send(&gToUsbThread, &msg);
}

// Fires once the user has confirmed past the file-found warning built from a
// gSynthRestorePeekComplete result (below). Sends eMsgCmdApplySynthSettingsRestore with no
// payload — the parsed settings are already staged on the USB thread
// (sSynthSettingsRestoreStaged), set by peek_synth_settings_restore().
static void on_synth_restore_confirmed(bool confirmed) {
    tMessageContent msg = {0};

    if (!confirmed) {
        return;
    }
    msg.cmd = eMsgCmdApplySynthSettingsRestore;
    msg_send(&gToUsbThread, &msg);
}

// Busy state for in-flight whole-slot device ops (load/save/new patch). Set when the op is enqueued,
// cleared when its completion response is drained off gToGuiThread. See reverse-queue-design.md.
static double sDeviceOpStartTime = 0.0; // glfwGetTime() of the oldest in-flight op — for the safety timeout

void device_op_begin(const char * label) {
    if (gDeviceOpInProgress == 0) {
        sDeviceOpStartTime = get_time_ms() / 1000.0;
    }
    gDeviceOpInProgress++;

    if (label != NULL) {
        COPY_STRING(gDeviceOpLabel, label);
    }
    synthlib_request_redraw();
}

void device_op_end(void) {
    if (gDeviceOpInProgress > 0) {
        gDeviceOpInProgress--;
    }
    synthlib_request_redraw();
}

// Dim the canvas + a small centred panel while a whole-slot device op is in flight. Drawn on top of
// the normal canvas (below the alert dialog, which only appears once the op has completed).
static void render_device_busy_overlay(void) {
    if (gDeviceOpInProgress <= 0) {
        return;
    }
    double renderW = get_render_width() / gGlobalGuiScale;
    double renderH = get_render_height() / gGlobalGuiScale;
    double boxW    = 240.0;
    double boxH    = 56.0;
    double boxX    = (renderW - boxW) / 2.0;
    double boxY    = (renderH - boxH) / 2.0;
    double titleH  = 24.0;

    draw_dialog_background_overlay();
    draw_panel_chrome(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH, "Please wait");
    render_text(mainArea, (tRectangle){{boxX + 10.0, boxY + titleH + 10.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, gDeviceOpLabel);
}

static void check_action_flags(void) {
    // Drain the reverse (USB->UI) response queue — see reverse-queue-design.md. One response per frame:
    // if a modal alert is already up, leave the queue untouched so it isn't clobbered (we'll drain the
    // next once it's dismissed); if more remain after handling one, self-wake so the next frame
    // continues rather than blocking in glfwWaitEvents.
    if (!alert_dialog_active()) {
        tMessageContent resp = {0};

        if (msg_receive(&gToGuiThread, eRcvPoll, &resp) == EXIT_SUCCESS) {
            const char * slash          = strrchr(resp.fileResultData.path, '/');
            const char * baseName       = (slash != NULL) ? slash + 1 : resp.fileResultData.path;
            char         alertMsg[1100] = {0};

            switch (resp.cmd) {
                case eRspFileLoad:
                    device_op_end();

                    if (resp.fileResultData.result != EXIT_SUCCESS) {
                        snprintf(alertMsg, sizeof(alertMsg),
                                 "Could not load \"%s\".\nThe file may be corrupt or not a valid G2 patch/performance.",
                                 baseName);
                        show_alert("Load Failed", alertMsg);
                    }
                    break;

                case eRspFileSave:
                    device_op_end();

                    if (resp.fileResultData.result != EXIT_SUCCESS) {
                        snprintf(alertMsg, sizeof(alertMsg),
                                 "Could not save \"%s\".\nCheck the location is writable and has free space.",
                                 baseName);
                        show_alert("Save Failed", alertMsg);
                    }
                    break;

                case eRspNewPatch:
                    device_op_end();

                    if (resp.fileResultData.result != EXIT_SUCCESS) {
                        show_alert("New Patch Failed", "The G2 did not accept the new patch. It may have gone offline.");
                    }
                    break;

                case eRspOfflineConflict:
                    show_offline_conflict_dialog(resp.offlineEditData.slotMask);
                    break;

                case eRspAlert:
                    show_alert(resp.alertData.title, resp.alertData.message);
                    break;

                case eRspStorePeek:
                {
                    // Peek data is in the gStorePeek* globals (set by the USB thread before it posted
                    // this); this response is just the "ready" nudge to open the overwrite confirm.
                    char title[64]    = {0};
                    char message[320] = {0};
                    bool isPerf       = gStorePeekIsPerf;

                    snprintf(title, sizeof(title), "Store %s to Bank %u, Location %u",
                             isPerf ? "Performance" : "Patch", gStorePeekBank + 1, gStorePeekLocation + 1);

                    if (gStorePeekFailed) {
                        show_alert(title, "Could not check what's currently at this location — the G2 may have gone offline. Try again.");
                    } else {
                        if (gStorePeekPopulated) {
                            snprintf(message, sizeof(message),
                                     "This location currently contains \"%s\". Storing will overwrite it with the current edit buffer %s. "
                                     "This cannot be undone.", gStorePeekName, isPerf ? "performance" : "patch");
                        } else {
                            snprintf(message, sizeof(message),
                                     "This location is currently empty. Store the current edit buffer %s there?", isPerf ? "performance" : "patch");
                        }
                        show_confirm(title, message, "Store...", on_store_confirmed);
                    }
                    break;
                }

                case eRspDeletePeek:
                {
                    char title[64]    = {0};
                    char message[320] = {0};
                    bool isPerf       = gDeletePeekIsPerf;

                    snprintf(title, sizeof(title), "Delete %s Bank %u, Location %u",
                             isPerf ? "Performance" : "Patch", gDeletePeekBank + 1, gDeletePeekLocation + 1);

                    if (gDeletePeekFailed) {
                        show_alert(title, "Could not check what's currently at this location — the G2 may have gone offline. Try again.");
                    } else {
                        if (gDeletePeekPopulated) {
                            snprintf(message, sizeof(message),
                                     "This location currently contains \"%s\". Deleting will erase it. This cannot be undone.", gDeletePeekName);
                        } else {
                            snprintf(message, sizeof(message), "This location is already empty. Nothing to delete — continue anyway?");
                        }
                        show_confirm(title, message, "Delete...", on_delete_confirmed);
                    }
                    break;
                }

                case eRspLoadPeek:
                {
                    char title[64] = {0};
                    bool isPerf    = gLoadPeekIsPerf;

                    snprintf(title, sizeof(title), "Load %s from Bank %u, Location %u",
                             isPerf ? "Performance" : "Patch", gLoadPeekBank + 1, gLoadPeekLocation + 1);

                    if (gLoadPeekFailed) {
                        show_alert(title, "Could not check what's at this location — the G2 may have gone offline. Try again.");
                    } else if (!gLoadPeekPopulated) {
                        show_alert(title, "This location is empty. There's nothing to load.");
                    } else {
                        // No confirm — loading from a file doesn't ask "replace the current edit buffer?" either.
                        on_load_confirmed(true);
                    }
                    break;
                }

                case eRspSynthRestorePeek:
                {
                    char message[400] = {0};

                    if (gSynthRestorePeekFailed) {
                        show_alert("Restore Synth Settings", gSynthRestorePeekErrorMessage);
                    } else {
                        snprintf(message, sizeof(message),
                                 "This will overwrite the current synth settings on the G2 with the contents of \"%s\" (Name: %s). "
                                 "This cannot be undone.", gSynthRestorePeekFileName, gSynthRestorePeekName);
                        show_confirm("Restore Synth Settings", message, "Restore...", on_synth_restore_confirmed);
                    }
                    break;
                }

                case eRspShowOpenRead:
                    // Deferred from a menu click so the browser opens from the render loop, not mid-callback.
                    open_file_browser_read(on_file_opened);
                    break;

                case eRspOpenPath:
                    // File > Open Recent. Goes through on_file_opened() exactly as the browser does,
                    // so a recent open records its path, updates File > Save's target and re-orders
                    // the recent list itself by the same route — no second copy of any of that.
                    on_file_opened(resp.patchFileData.filePath);
                    break;

                case eRspShowOpenWrite:
                {
                    uint32_t slot                              = gSlot;
                    char     patchName[CLAVIA_NAME_SIZE + 1]   = {0};
                    char     defaultName[CLAVIA_NAME_SIZE + 6] = {0}; // name (16) + extension (5) + null

                    if (gGlobalSettings.perfMode == 1) {
                        if (gGlobalSettings.perfName[0] != '\0') {
                            snprintf(defaultName, sizeof(defaultName), "%s.prf2", gGlobalSettings.perfName);
                        } else {
                            COPY_STRING(defaultName, "performance.prf2");
                        }
                    } else {
                        COPY_STRING(patchName, gGlobalSettings.slot[slot].patchName);

                        if (patchName[0] != '\0') {
                            snprintf(defaultName, sizeof(defaultName), "%s.pch2", patchName);
                        } else {
                            snprintf(defaultName, sizeof(defaultName), "patch.pch2");
                        }
                    }
                    open_file_browser_write(on_file_saved, defaultName);
                    break;
                }

                case eRspSaveToCurrentPath:
                {
                    // File > Save: straight back to the remembered path, no browser. The menu only
                    // offers this once there IS one, but re-check here — the drain runs a frame or
                    // more after the click, and a slot change in between would move the goalposts.
                    const char * path = (gGlobalSettings.perfMode == 1) ? gSavedPerfPath : gSavedPatchPath[gSlot];

                    if (path[0] == '\0') {
                        open_file_browser_write(on_file_saved, "patch.pch2");  // Nothing to save back to
                    } else {
                        on_file_saved(path);
                    }
                    break;
                }

                default:
                    break;
            }

            if (msg_count(&gToGuiThread) > 0) {
                wake_glfw(); // more queued — come back next frame rather than blocking in glfwWaitEvents
            }
        }
    }

    // Safety net: if a device op's completion response never arrives (e.g. the G2 disconnected in the
    // gap between enqueue and processing), don't leave the GUI locked forever — force-clear after a
    // generous timeout. Whole-slot ops complete in well under 1s in practice.
    if ((gDeviceOpInProgress > 0) && ((glfwGetTime() - sDeviceOpStartTime) > 5.0)) {
        LOG_ERROR("Device op busy-state timed out — force-clearing\n");
        gDeviceOpInProgress = 0;
        synthlib_request_redraw();
    }

    if (gNeedFocus == true) {
        gNeedFocus = false;
        glfwFocusWindow((GLFWwindow *)synthlib_window());
    }
    // Completion alerts (bank backup/restore/store/delete/load/synth) and peek→confirm prompts now all
    // arrive on the reverse queue (see the drain switch above) — their poll flags were retired. The
    // gBank*IsPerf / gBank*IsEverything flags stay (the progress overlays read them); the gStore/Delete/
    // Load/SynthRestorePeek* data globals stay too (the drain reads them to build the confirm dialog).
}

// Helper: draw a fixed-width dropdown trigger button, return updated x.
static double render_dropdown(double x, double y, double btnH,
                              const char * valStr, const char * widestVal,
                              tRectangle * rect) {
    *rect = draw_button(mainArea, (tRectangle){{x, y}, {get_text_width((char *)widestVal, btnH, eCache) + 8.0, btnH}},
                        (char *)valStr, (tRgb)RGB_BACKGROUND_GREY);
    return x + rect->size.w;
}

// Helper: MIDI note number → note name string (e.g. 0 → "C-1", 60 → "C4")
static void midi_note_name_str(uint8_t note, char * buf, size_t bufLen) {
    static const char * names[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
    int                 octave  = (int)(note / 12) - 1;

    snprintf(buf, bufLen, "%s%d", names[note % 12], octave);
}

static void render_ss_section(double boxX, double boxW, double margin, double * y,
                              double rowH, double btnH,
                              const tSynthSettingItem * items, int count) {
    double itemW = (boxW - margin * 2.0) / 2.0;
    double x     = 0.0;
    int    i     = 0;

    for (i = 0; i < count; i++) {
        x              = boxX + margin + (i % 2) * itemW;

        if (i > 0 && (i % 2) == 0) {
            *y += rowH;
        }
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, *y + 2.0}, {BLANK_SIZE, btnH}}, (char *)items[i].label);
        x             += get_text_width((char *)items[i].label, btnH, eCache) + 4.0;
        *items[i].rect = draw_button(mainArea,
                                     (tRectangle){{x, *y}, {get_text_width((char *)items[i].widest, btnH, eCache) + 8.0, btnH}},
                                     items[i].get_str(),
                                     items[i].get_colour());
    }

    *y += rowH;
}

static double render_pp_row(double x, double y, double btnH,
                            const tPatchParamItem * items, int count) {
    int i = 0;

    for (i = 0; i < count; i++) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, (char *)items[i].label);
        x                                += get_text_width((char *)items[i].label, btnH, eCache) + 4.0;
        gPatchParamRects[items[i].rectId] = draw_button(mainArea,
                                                        (tRectangle){{x, y}, {get_text_width((char *)items[i].widest, btnH, eCache) + 8.0, btnH}},
                                                        items[i].get_str(),
                                                        items[i].get_colour());
        x                                += get_text_width((char *)items[i].widest, btnH, eCache) + 8.0 + 16.0;
    }

    return x;
}

static void midi_chan_str(uint8_t val, char * buf, size_t bufLen) {
    if (val >= 0x10) {
        snprintf(buf, bufLen, "Off");
    } else {
        snprintf(buf, bufLen, "%u", (unsigned)val + 1u);
    }
}

// ---------------------------------------------------------------------------
// Shared popup-panel chrome — every dialog-style panel (Synth/Perf/Patch
// Settings, Patch Notes, Mutator) draws the same bordered box + inset title
// bar + right-aligned Close button. Pulled out here so the border-inset and
// close-button-position fixes only need to exist in one place.
// ---------------------------------------------------------------------------

// Dims the module canvas behind a modal dialog. Not used by Mutator, which floats
// alongside the canvas rather than blocking it.

// Draws the bordered box and the title bar (inset from the border so it never paints
// over the white/black border line) with white title text. Returns the full-width,
// non-inset title bar rectangle — callers that need it as a drag handle (Mutator) can
// hit-test against that; everyone else can ignore the return value.

// Draws the standard "Close" button, right-aligned in the title bar at the app's
// standard inset, darkened while closePressed is true. Returns its rectangle for the
// caller's own hit-testing (this function does not track press state itself, since
// each panel already has its own closePressed bool wired into its mouse handler).

static void render_patch_settings_panel(void) {
    static const char * slotLabel[4] = {"A", "B", "C", "D"};
    double              boxW         = 600.0;
    double              boxH         = 453.0;
    double              boxX         = 0.0;
    double              boxY         = 0.0;
    double              margin       = 10.0;
    double              titleH       = 24.0;
    double              rowH         = 26.0;
    double              secH         = 18.0;
    double              btnH         = STANDARD_BUTTON_TEXT_HEIGHT;
    double              y            = 0.0;
    double              colW         = 0.0;
    double              x            = 0.0;
    int                 i            = 0;
    char                buf[16]      = {0};

    if (!gPatchSettingsEdit.active) {
        return;
    }
    // FLOATING, NOT MODAL (2026-08-20). The position comes from the panel state instead of being
    // recomputed as (renderW - boxW) / 2 every frame — which is what made these three impossible to
    // move — and there is no background overlay, so the patch stays visible and clickable underneath
    // and a second panel can share the screen. Same treatment the Virtual Keyboard already had, and
    // the reason renderW/renderH are gone from here: centring was all they were for. See SynthLib
    // floatingPanel.h.
    tRectangle          box          = floating_panel_place(&gPatchSettingsEdit.panel, boxW, boxH);

    boxX                                  = box.coord.x;
    boxY                                  = box.coord.y;
    y                                     = boxY + titleH + margin;

    gPatchSettingsEdit.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Synth Settings");
    gSettingsPanelRects.close             = draw_panel_close_button(mainArea, box, gSettingsPanelRects.closePressed);
    gPatchSettingsEdit.panel.closeRect    = gSettingsPanelRects.close;   // carve it out of the title-bar drag

    // ── Synth Name ─────────────────────────────────────────────────
    {
        char displayBuf[CLAVIA_NAME_SIZE + 2] = {0};

        x  = boxX + margin;
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Name:");
        x += get_text_width((char *)"Name:", btnH, eCache) + 4.0;

        if (gSynthNameEdit.active) {
            uint32_t cp = gSynthNameEdit.cursorPos;
            memcpy(displayBuf, gSynthNameEdit.buffer, cp);
            displayBuf[cp]                = '|';
            memcpy(&displayBuf[cp + 1], &gSynthNameEdit.buffer[cp], strlen(gSynthNameEdit.buffer) - cp + 1);
            gSettingsPanelRects.synthName = draw_button(mainArea, (tRectangle){{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}}, displayBuf, (tRgb)RGB_WHITE);
        } else {
            snprintf(displayBuf, sizeof(displayBuf), "%s", gSynthSettings.name);
            gSettingsPanelRects.synthName = draw_button(mainArea, (tRectangle){{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}}, displayBuf, (tRgb)RGB_BACKGROUND_GREY);
        }
    }
    y   += rowH;

    // ── MIDI Channels ──────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "MIDI Channels");
    y   += secH;
    colW = (boxW - margin * 2.0) / 4.0;

    for (i = 0; i < 4; i++) {
        x  = boxX + margin + i * colW;
        snprintf(buf, sizeof(buf), "%c:", slotLabel[i][0]);
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, buf);
        x += get_text_width((char *)"A:", btnH, eCache) + 4.0;
        midi_chan_str(gSynthSettings.midiChanSlot[i], buf, sizeof(buf));
        render_dropdown(x, y, btnH, buf, "Off", &gSettingsPanelRects.midiChan[i]);
    }

    y   += rowH;

    // ── Global + SysEx ─────────────────────────────────────────────
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSGlobal, kSSGlobalCount);

    // ── Options ────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Options");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSOptions, kSSOptionsCount);

    // ── Tuning ─────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Tuning");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSTuning, kSSTuningCount);

    // ── Pedal ──────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Pedal");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSPedal, kSSPedalCount);

    // ── Sort Mode ──────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Sort Mode");
    y   += secH;
    render_ss_section(boxX, boxW, margin, &y, rowH, btnH, kSSSort, kSSSortCount);
}

static void render_patch_params_panel(void) {
    if (!gPatchParamsEdit.active) {
        return;
    }
    uint32_t   slot          = gPatchParamsEdit.slot;
    double     boxW          = 680.0;
    double     boxH          = 320.0;
    tRectangle box           = floating_panel_place(&gPatchParamsEdit.panel, boxW, boxH);
    double     boxX          = box.coord.x;
    double     boxY          = box.coord.y;
    double     margin        = 10.0;
    double     titleH        = 24.0;
    double     rowH          = 26.0;
    double     secH          = 18.0;
    double     btnH          = STANDARD_BUTTON_TEXT_HEIGHT;
    double     y             = boxY + titleH + margin;
    double     x             = 0.0;
    double     dialH         = 0.0;
    tModule *  sustMod       = get_module_slot(slot, (uint32_t)locationMorph, patchModuleSustain);
    tModule *  vibMod        = get_module_slot(slot, (uint32_t)locationMorph, patchModuleVibrato);
    tModule *  glideMod      = get_module_slot(slot, (uint32_t)locationMorph, patchModuleGlide);
    uint8_t    sustainPedal  = sustMod ? sustMod->param[0][SUSTAIN_PEDAL].value : 0;
    int8_t     octaveShift   = sustMod ? (int8_t)sustMod->param[0][OCTAVE_SHIFT].value : 0;
    uint8_t    vibratoRate   = vibMod ? vibMod->param[0][VIBRATO_RATE].value : 0;
    uint8_t    vibratoAmount = vibMod ? vibMod->param[0][VIBRATO_DEPTH].value : 0;
    uint8_t    glideTime     = glideMod ? glideMod->param[0][GLIDE_SPEED].value : 0;
    char       buf[16]       = {0};

    gPatchParamsEdit.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Patch Settings");
    gPatchParamClose                    = draw_panel_close_button(mainArea, box, gPatchParamClosePressed);
    gPatchParamsEdit.panel.closeRect    = gPatchParamClose;   // carve it out of the title-bar drag

    // ── Slot buttons in title bar ──────────────────────────────────
    {
        static const char * slotLabels[MAX_SLOTS] = {"A", "B", "C", "D"};
        double              slotBtnW              = get_text_width((char *)"A", btnH, eCache) /* + 6.0*/;
        // Right-aligned against the panel edge, now that the close control has moved top left.
        double              slotX                 = boxX + boxW - 8.0 - BORDER_LINE_WIDTH - ((slotBtnW + 8.0) * MAX_SLOTS);

        for (uint32_t s = 0; s < MAX_SLOTS; s++) {
            tRgb col = (s == slot) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY;
            gPatchParamSlots[s] = draw_button(mainArea,
                                              (tRectangle){{slotX + s * (slotBtnW + 8.0), boxY + 4.0}, {slotBtnW, btnH}},
                                              slotLabels[s], col);
        }
    }

    // ── Sustain Pedal + Octave Shift ───────────────────────────────
    {
        x                                = boxX + margin;
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Sustain Pedal:");
        x                               += get_text_width((char *)"Sustain Pedal:", btnH, eCache) + 4.0;
        gPatchParamRects[pPSustainPedal] = draw_button(mainArea,
                                                       (tRectangle){{x, y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                       sustainPedal ? "On" : "Off",
                                                       sustainPedal ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);

        x                                = boxX + boxW / 2.0;
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Octave Shift:");
        x                               += get_text_width((char *)"Octave Shift:", btnH, eCache) + 4.0;
        snprintf(buf, sizeof(buf), "%+d", (int)octaveShift);
        render_dropdown(x, y, btnH, buf, "+2", &gPatchParamRects[pPOctaveShift]);
    }
    y                                += rowH;

    // ── Arpeggiator ────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Arpeggiator");
    y                                += secH;
    render_pp_row(boxX + margin, y, btnH, kPPArp, kPPArpCount);
    y                                += rowH;

    // ── Vibrato ────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Vibrato");
    y                                += secH;
    x                                 = render_pp_row(boxX + margin, y, btnH, kPPVibrato, kPPVibratoCount);
    // Dial-anchored: the rect is the circle, and its label and value are drawn in the two text
    // rows above it - so the dial goes two rows below where the block used to start.
    dialH                             = 20.0;
    snprintf(buf, sizeof(buf), "%u cnt", (unsigned)vibratoAmount);
    gPatchParamRects[pPVibratoAmount] = render_dial_with_text(mainArea, (tRectangle){{x, (y - 10.0) + (btnH * 2.0)}, {20.0, dialH}}, "Amount", buf, btnH, vibratoAmount, 100, 0, (tRgb)RGB_BACKGROUND_GREY);
    x                                += get_text_width((char *)"100 cnt", btnH, eCache) + 8.0;
    snprintf(buf, sizeof(buf), "%.2f Hz", 4.0 + (vibratoRate / 127.0) * 4.0);
    gPatchParamRects[pPVibratoRate]   = render_dial_with_text(mainArea, (tRectangle){{x, (y - 10.0) + (btnH * 2.0)}, {20.0, dialH}}, "Rate", buf, btnH, vibratoRate, 127, 0, (tRgb)RGB_BACKGROUND_GREY);
    y                                += rowH;

    // ── Glide ──────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Glide");
    y                                += secH;
    x                                 = render_pp_row(boxX + margin, y, btnH, kPPGlide, kPPGlideCount);
    gPatchParamRects[pPGlideTime]     = render_dial_with_text(mainArea, (tRectangle){{x, (y - 10.0) + (btnH * 2.0)}, {20.0, dialH}}, "Time", get_glide_time_str(glideTime), btnH, glideTime, 127, 0, (tRgb)RGB_BACKGROUND_GREY);
    y                                += rowH;

    // ── Bend ───────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Bend");
    y                                += secH;
    render_pp_row(boxX + margin, y, btnH, kPPBend, kPPBendCount);

    (void)y;
}

static void render_perf_settings_panel(void) {
    if (!gPerfSettingsEdit.active) {
        return;
    }
    double     boxW                     = 700.0;
    double     boxH                     = 390.0;
    tRectangle box                      = floating_panel_place(&gPerfSettingsEdit.panel, boxW, boxH);
    double     boxX                     = box.coord.x;
    double     boxY                     = box.coord.y;
    double     margin                   = 10.0;
    double     titleH                   = 24.0;
    double     rowH                     = 26.0;
    double     secH                     = 18.0;
    double     btnH                     = STANDARD_BUTTON_TEXT_HEIGHT;
    double     y                        = boxY + titleH + margin;
    double     colX[kPSSlotToggleCount] = {0.0, 0.0, 0.0};
    int        i                        = 0;
    int        col                      = 0;
    char       buf[32]                  = {0};
    char       note[8]                  = {0};
    char       loNote[8]                = {0};
    char       hiNote[8]                = {0};
    char       rangeBuf[18]             = {0};

    gPerfSettingsEdit.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Performance Settings");
    gPerfSettingsPanelRects.close        = draw_panel_close_button(mainArea, box, gPerfSettingsPanelRects.closePressed);
    gPerfSettingsEdit.panel.closeRect    = gPerfSettingsPanelRects.close;   // carve it out of the title-bar drag

    // ── Perf Name ──────────────────────────────────────────────────
    {
        char   nameBuf[CLAVIA_NAME_SIZE + 2] = {0};
        double x                             = boxX + margin;
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Name:");
        x += get_text_width((char *)"Name:", btnH, eCache) + 4.0;
        snprintf(nameBuf, sizeof(nameBuf), "%s", gGlobalSettings.perfName);
        draw_button(mainArea, (tRectangle){{x, y}, {get_text_width(LONGEST_PATCH_NAME, btnH, eCache), btnH}},
                    nameBuf, (tRgb)RGB_BACKGROUND_GREY);
    }
    y                                   += rowH;

    // ── Master Clock ───────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Master Clock");
    y                                   += secH;

    {
        // blockH is the whole BPM readout + dial, which the Running button centres against and
        // the row advance uses. The dial itself is one text row down from the top of that, since
        // render_dial_with_text() is dial-anchored and draws the BPM string above it.
        double blockH = 48.0;
        double x      = boxX + margin;
        snprintf(buf, sizeof(buf), "%u BPM", (unsigned)gGlobalSettings.masterClock);
        gPerfSettingsPanelRects.masterClock        = render_dial_with_text(mainArea, (tRectangle){{x, y + STANDARD_BUTTON_TEXT_HEIGHT}, {20.0, 20.0}}, NULL, buf, STANDARD_BUTTON_TEXT_HEIGHT, gGlobalSettings.masterClock >= 30 ? gGlobalSettings.masterClock - 30 : 0, 211, 0, (tRgb)RGB_BACKGROUND_GREY);
        x                                         += 20.0 + 12.0;
        gPerfSettingsPanelRects.masterClockRunning = draw_button(mainArea,
                                                                 (tRectangle){{x, y + (blockH - btnH) / 2.0}, {get_text_width((char *)"Stopped", btnH, eCache) + 8.0, btnH}},
                                                                 gGlobalSettings.masterClockRunning ? "Running" : "Stopped",
                                                                 gGlobalSettings.masterClockRunning ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        y                                         += blockH + 4.0;
    }

    // ── Slots ──────────────────────────────────────────────────────
    set_rgb_colour((tRgb)RGB_GREY_7);
    render_text(mainArea, (tRectangle){{boxX + margin, y}, {BLANK_SIZE, btnH}}, "Slots");
    y      += secH;

    double labelColW = get_text_width((char *)"Slot X:", btnH, eCache) + 8.0;
    double dropW     = get_text_width((char *)"On", btnH, eCache) + 16.0;
    double noteDropW = get_text_width((char *)"C#-1", btnH, eCache) + 10.0;
    double colEn     = boxX + margin + labelColW;
    double colKbd    = colEn + dropW + 8.0;
    double colHld    = colKbd + dropW + 30.0;
    double colLo     = colHld + dropW + 16.0;
    double colHi     = colLo + noteDropW + 8.0;
    double colRng    = colHi + noteDropW + 12.0;

    colX[0] = colEn;
    colX[1] = colKbd;
    colX[2] = colHld;

    // Column headers
    set_rgb_colour((tRgb)RGB_BLACK);
    render_text(mainArea, (tRectangle){{colEn, y}, {BLANK_SIZE, btnH}}, "Enable");
    render_text(mainArea, (tRectangle){{colKbd, y}, {BLANK_SIZE, btnH}}, "Keyboard");
    render_text(mainArea, (tRectangle){{colHld, y}, {BLANK_SIZE, btnH}}, "Hold");
    render_text(mainArea, (tRectangle){{colLo, y}, {BLANK_SIZE, btnH}}, "Lower");
    render_text(mainArea, (tRectangle){{colHi, y}, {BLANK_SIZE, btnH}}, "Upper");

    // Keyboard Range global toggle — right side of header
    {
        double x = colRng;
        render_text(mainArea, (tRectangle){{x, y + 2.0}, {BLANK_SIZE, btnH}}, "Kbd Range:");
        x                                    += get_text_width((char *)"Kbd Range:", btnH, eCache) + 4.0;
        gPerfSettingsPanelRects.keyboardRange = draw_button(mainArea,
                                                            (tRectangle){{x, y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                            gPerfSettings.keyboardRange ? "On" : "Off",
                                                            gPerfSettings.keyboardRange ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
    }
    y      += rowH;

    // Slot rows A–D
    static const char * slotLabel[] = {"Slot A:", "Slot B:", "Slot C:", "Slot D:"};

    for (i = 0; i < MAX_SLOTS; i++) {
        set_rgb_colour((tRgb)RGB_BLACK);
        render_text(mainArea, (tRectangle){{boxX + margin, y + 2.0}, {BLANK_SIZE, btnH}}, (char *)slotLabel[i]);

        for (col = 0; col < kPSSlotToggleCount; col++) {
            kPSSlotToggles[col].rects[i] = draw_button(mainArea,
                                                       (tRectangle){{colX[col], y}, {get_text_width((char *)"On", btnH, eCache) + 8.0, btnH}},
                                                       kPSSlotToggles[col].get_str(i),
                                                       kPSSlotToggles[col].get_colour(i));
        }

        midi_note_name_str(gPerfSettings.slot[i].rangeLower, note, sizeof(note));
        render_dropdown(colLo, y, btnH, note, "C#-1", &gPerfSettingsPanelRects.rangeLower[i]);

        midi_note_name_str(gPerfSettings.slot[i].rangeUpper, note, sizeof(note));
        render_dropdown(colHi, y, btnH, note, "C#-1", &gPerfSettingsPanelRects.rangeUpper[i]);

        midi_note_name_str(gPerfSettings.slot[i].rangeLower, loNote, sizeof(loNote));
        midi_note_name_str(gPerfSettings.slot[i].rangeUpper, hiNote, sizeof(hiNote));
        snprintf(rangeBuf, sizeof(rangeBuf), "%s - %s", loNote, hiNote);
        render_text(mainArea, (tRectangle){{colRng, y + 2.0}, {BLANK_SIZE, btnH}}, rangeBuf);

        y += rowH;
    }

    (void)buf;
}

static void render_bank_backup_progress(void) {
    if (!gBankBackupActive) {
        return;
    }
    double renderW      = get_render_width() / gGlobalGuiScale;
    double renderH      = get_render_height() / gGlobalGuiScale;
    double boxW         = 360.0;
    double boxH         = 90.0;
    double boxX         = (renderW - boxW) / 2.0;
    double boxY         = (renderH - boxH) / 2.0;
    double margin       = 10.0;
    double titleH       = 24.0;
    char   lineBuf[128] = {0};
    bool   isPerf       = gBankBackupIsPerf;
    bool   isEverything = gBankBackupIsEverything;

    draw_dialog_background_overlay();
    draw_panel_chrome(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH,
                      isEverything ? "Backup Everything" : (isPerf ? "Backing Up Performance Bank" : "Backing Up Patch Bank"));

    if (isEverything) {
        snprintf(lineBuf, sizeof(lineBuf), "%s Bank %u of %u - location %u / %u",
                 isPerf ? "Performance" : "Patch", gBankBackupBank + 1,
                 isPerf ? NUM_PERF_BANKS : NUM_PATCH_BANKS, gBankBackupLocation + 1, NUM_LOCATIONS_PER_BANK);
    } else {
        snprintf(lineBuf, sizeof(lineBuf), "Bank %u - location %u / %u",
                 gBankBackupBank + 1, gBankBackupLocation + 1, NUM_LOCATIONS_PER_BANK);
    }
    render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, lineBuf);

    snprintf(lineBuf, sizeof(lineBuf), "%u %s%s written so far",
             gBankBackupWritten, isPerf ? "performance" : "patch",
             gBankBackupWritten == 1 ? "" : (isPerf ? "s" : "es"));
    render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin + STANDARD_TEXT_HEIGHT + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, lineBuf);

    // Progress bar
    double barY = boxY + boxH - margin - 8.0;
    double barW = boxW - margin * 2.0;
    double frac = (double)(gBankBackupLocation + 1) / (double)NUM_LOCATIONS_PER_BANK;

    set_rgb_colour((tRgb)RGB_GREY_9);
    render_rectangle(mainArea, (tRectangle){{boxX + margin, barY}, {barW, 8.0}});
    set_rgb_colour((tRgb)RGB_GREEN_ON);
    render_rectangle(mainArea, (tRectangle){{boxX + margin, barY}, {barW * frac, 8.0}});
}

static void render_bank_restore_progress(void) {
    if (!gBankRestoreActive) {
        return;
    }
    double renderW      = get_render_width() / gGlobalGuiScale;
    double renderH      = get_render_height() / gGlobalGuiScale;
    double boxW         = 360.0;
    double boxH         = 90.0;
    double boxX         = (renderW - boxW) / 2.0;
    double boxY         = (renderH - boxH) / 2.0;
    double margin       = 10.0;
    double titleH       = 24.0;
    char   lineBuf[128] = {0};
    bool   isPerf       = gBankRestoreIsPerf;
    bool   isEverything = gBankRestoreIsEverything;

    draw_dialog_background_overlay();
    draw_panel_chrome(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH,
                      isEverything ? "Restore Everything" : (isPerf ? "Restoring Performance Bank" : "Restoring Patch Bank"));

    if (isEverything) {
        snprintf(lineBuf, sizeof(lineBuf), "%s Bank %u of %u - location %u / %u",
                 isPerf ? "Performance" : "Patch", gBankRestoreBank + 1,
                 isPerf ? NUM_PERF_BANKS : NUM_PATCH_BANKS, gBankRestoreLocation + 1, NUM_LOCATIONS_PER_BANK);
    } else {
        snprintf(lineBuf, sizeof(lineBuf), "Bank %u - location %u / %u",
                 gBankRestoreBank + 1, gBankRestoreLocation + 1, NUM_LOCATIONS_PER_BANK);
    }
    render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, lineBuf);

    snprintf(lineBuf, sizeof(lineBuf), "%u %s%s written so far",
             gBankRestoreWritten, isPerf ? "performance" : "patch",
             gBankRestoreWritten == 1 ? "" : (isPerf ? "s" : "es"));
    render_text(mainArea, (tRectangle){{boxX + margin, boxY + titleH + margin + STANDARD_TEXT_HEIGHT + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, lineBuf);

    // Progress bar
    double barY = boxY + boxH - margin - 8.0;
    double barW = boxW - margin * 2.0;
    double frac = (double)(gBankRestoreLocation + 1) / (double)NUM_LOCATIONS_PER_BANK;

    set_rgb_colour((tRgb)RGB_GREY_9);
    render_rectangle(mainArea, (tRectangle){{boxX + margin, barY}, {barW, 8.0}});
    set_rgb_colour((tRgb)RGB_GREEN_ON);
    render_rectangle(mainArea, (tRectangle){{boxX + margin, barY}, {barW * frac, 8.0}});
}

static void render_patch_notes_edit(void) {
    if (!gPatchNotesEdit.active) {
        return;
    }
    double renderW      = get_render_width() / gGlobalGuiScale;
    double renderH      = get_render_height() / gGlobalGuiScale;
    double boxW         = 700.0;
    double boxH         = 500.0;
    double boxX         = (renderW - boxW) / 2.0;
    double boxY         = (renderH - boxH) / 2.0;
    double margin       = 10.0;
    double titleH       = 24.0;
    double lineH        = STANDARD_TEXT_HEIGHT + 3.0;
    double hintH        = STANDARD_TEXT_HEIGHT + 12.0; // extra headroom below the button/text baseline so descenders (g, y, p) aren't clipped by the border
    double textY0       = boxY + titleH + margin;
    double textX        = boxX + margin;
    double textW        = boxW - margin * 2.0;
    double maxTextH     = boxH - titleH - hintH - margin * 3.0;
    char   countBuf[32] = {0};

    double btnH         = STANDARD_BUTTON_TEXT_HEIGHT;

    draw_dialog_background_overlay();
    draw_panel_chrome(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH, "Patch Notes");
    gPatchNotesCloseRect = draw_panel_close_button(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, gPatchNotesClosePressed);

    // Character count
    snprintf(countBuf, sizeof(countBuf), "%zu / %d", strlen(gPatchNotesEdit.buffer), PATCH_NOTES_SIZE);
    set_rgb_colour((tRgb)RGB_GREY_9);
    render_text(mainArea, (tRectangle){{boxX + boxW / 2.0 - 30.0, boxY + 6.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, countBuf);

    // Cache geometry for click-to-cursor and keyboard navigation
    gNoteTextX           = textX;
    gNoteTextY0          = textY0;
    gNoteLineH           = lineH;
    gNoteTextW           = textW;
    gNoteTextHParam      = STANDARD_TEXT_HEIGHT;

    build_note_visual_lines(gPatchNotesEdit.buffer, textW, STANDARD_TEXT_HEIGHT);

    int    cursorPos    = (int)gPatchNotesEdit.cursorPos;
    int    cursorLine   = find_note_cursor_line(cursorPos);
    int    visLines     = (int)(maxTextH / lineH);

    // Keep scroll so the cursor line is always visible
    if (cursorLine < gNoteScrollLine) {
        gNoteScrollLine = cursorLine;
    }

    if (cursorLine >= gNoteScrollLine + visLines) {
        gNoteScrollLine = cursorLine - visLines + 1;
    }

    if (gNoteScrollLine < 0) {
        gNoteScrollLine = 0;
    }
    // Text content area background
    set_rgb_colour((tRgb)RGB_WHITE);
    render_rectangle(mainArea, (tRectangle){{boxX + 1, boxY + titleH}, {boxW - 2, boxH - titleH - hintH - 1}});

    {
        const char * buf = gPatchNotesEdit.buffer;
        double       y   = textY0;

        for (int i = gNoteScrollLine; i < gNoteLineCount && i < gNoteScrollLine + visLines; i++) {
            int  start                             = gNoteLines[i].bufStart;
            int  end                               = gNoteLines[i].bufEnd;
            int  len                               = end - start;

            char displayLine[PATCH_NOTES_SIZE + 4] = {0};

            if (i == cursorLine) {
                int col = cursorPos - start;

                if (col < 0) {
                    col = 0;
                }

                if (col > len) {
                    col = len;
                }
                strncpy(displayLine, buf + start, col);
                displayLine[col]     = '|';
                strncpy(displayLine + col + 1, buf + start + col, len - col);
                displayLine[len + 1] = '\0';
            } else {
                strncpy(displayLine, buf + start, len);
                displayLine[len] = '\0';
            }
            set_rgb_colour((tRgb)RGB_BLACK);
            render_text(mainArea, (tRectangle){{textX, y}, {textW, STANDARD_TEXT_HEIGHT}}, displayLine);
            y += lineH;
        }
    }

    // Bottom bar: Discard Edits button + hint text. Inset so it doesn't paint over the panel's
    // bottom/left/right border line, same as the title bar above.
    set_rgb_colour((tRgb)RGB_GREY_3);
    render_rectangle(mainArea, (tRectangle){{boxX + BORDER_LINE_WIDTH, boxY + boxH - hintH}, {boxW - 2.0 * BORDER_LINE_WIDTH, hintH - BORDER_LINE_WIDTH}});

    double btnY       = boxY + boxH - hintH + (hintH - btnH) / 2.0 - 2.0; // draw_button's internal padding sits its text a couple px lower than render_text at the same y - nudge up so "Discard Edits" lines up with the hint text baseline
    double btnX       = boxX + margin;
    tRgb   discardCol = gPatchNotesDiscardPressed ? (tRgb)RGB_GREY_7 : (tRgb)RGB_BACKGROUND_GREY;
    gPatchNotesDiscardRect = draw_button(mainArea,
                                         (tRectangle){{btnX, btnY}, {get_text_width((char *)"Discard Edits", btnH, eCache) + 4.0, btnH}},
                                         (char *)"Discard Edits", discardCol);
    btnX                  += gPatchNotesDiscardRect.size.w + 12.0;
    set_rgb_colour((tRgb)RGB_WHITE);
    render_text(mainArea, (tRectangle){{btnX, boxY + boxH - hintH + (hintH - STANDARD_TEXT_HEIGHT) / 2.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}},
                "Arrows/Click=move   Enter=newline   Esc=close without saving");
}

int note_editor_cursor_move_line(int cursorPos, int delta) {
    if (gNoteLineCount == 0) {
        return cursorPos;
    }
    int curLine = find_note_cursor_line(cursorPos);
    int col     = cursorPos - gNoteLines[curLine].bufStart;
    int newLine = curLine + delta;

    if (newLine < 0) {
        newLine = 0;
    }

    if (newLine >= gNoteLineCount) {
        newLine = gNoteLineCount - 1;
    }
    int newLen  = gNoteLines[newLine].bufEnd - gNoteLines[newLine].bufStart;
    return gNoteLines[newLine].bufStart + (col < newLen ? col : newLen);
}

int note_editor_cursor_line_home(int cursorPos) {
    if (gNoteLineCount == 0) {
        return 0;
    }
    return gNoteLines[find_note_cursor_line(cursorPos)].bufStart;
}

int note_editor_cursor_line_end(int cursorPos) {
    if (gNoteLineCount == 0) {
        return 0;
    }
    return gNoteLines[find_note_cursor_line(cursorPos)].bufEnd;
}

int note_editor_cursor_from_click(double logicalX, double logicalY) {
    if (gNoteLineCount == 0) {
        return -1;
    }
    double       relY    = logicalY - gNoteTextY0;

    if (relY < 0) {
        return -1;
    }
    int          lineIdx = gNoteScrollLine + (int)(relY / gNoteLineH);

    if (lineIdx >= gNoteLineCount) {
        lineIdx = gNoteLineCount - 1;
    }

    if (lineIdx < 0) {
        return -1;
    }
    int          start   = gNoteLines[lineIdx].bufStart;
    int          end     = gNoteLines[lineIdx].bufEnd;
    const char * buf     = gPatchNotesEdit.buffer;
    double       relX    = logicalX - gNoteTextX;
    char         tmp[PATCH_NOTES_SIZE + 1];

    for (int col = 0; col <= end - start; col++) {
        strncpy(tmp, buf + start, col);
        tmp[col] = '\0';

        if (get_text_width(tmp, gNoteTextHParam, eNoCache) > relX) {
            if (col > 0) {
                strncpy(tmp, buf + start, col - 1);
                tmp[col - 1] = '\0';
                double wPrev = get_text_width(tmp, gNoteTextHParam, eNoCache);
                strncpy(tmp, buf + start, col);
                tmp[col]     = '\0';
                double wCur  = get_text_width(tmp, gNoteTextHParam, eNoCache);
                return start + ((relX - wPrev < wCur - relX) ? col - 1 : col);
            }
            return start;
        }
    }

    return start + (end - start);
}

// Renders one full frame and swaps buffers. Extracted from do_graphics_loop's
// inlined render block so the backdoor SCREENSHOT command can force a
// synchronous frame (see backdoor_screenshot()).
// ── TEMPORARY DEBUG AID — mouse crosshair ───────────────────────────────────
// Draws full-width/full-height lines through the cursor plus a numeric readout,
// for validating button hit points against their registered rectangles.
//
// Deliberately uses get_global_gui_scaled_mouse_coord() — the SAME call the
// click handlers use — so the number shown is literally the coordinate that
// gets compared against each rectangle, not an independently-derived one that
// could agree by luck while the real dispatch path disagrees.
//
// NOTE: this is mainArea (unscrolled) space. Top bar, menu bar and panel
// buttons live here, so their hit rects can be read off directly. Module-area
// elements are scroll/zoom-adjusted afterwards, so for those the crosshair
// shows the pre-adjustment cursor position, not the module-local one.
//
// Debug builds only (ENABLE_MOUSE_CROSSHAIR lives in defs.h), and OFF until F9
// is pressed — the lines sit above even the modal alert, so leaving it on by
// default would be intrusive.
#ifdef ENABLE_MOUSE_CROSSHAIR

static bool gShowMouseCrosshair = false;

void toggle_mouse_crosshair(void) {
    gShowMouseCrosshair = !gShowMouseCrosshair;
    LOG_DEBUG("Mouse crosshair %s\n", gShowMouseCrosshair ? "ON" : "OFF");
}

static void render_mouse_crosshair(void) {
    tCoord coord      = {0};
    char   buff[64]   = {0};
    double logicalW   = 0.0;
    double logicalH   = 0.0;
    double thickness  = 0.0;
    double textHeight = STANDARD_TEXT_HEIGHT;
    double textX      = 0.0;
    double textY      = 0.0;

    if (gShowMouseCrosshair == false) {
        return;
    }
    logicalW  = get_render_width() / gGlobalGuiScale;
    logicalH  = get_render_height() / gGlobalGuiScale;
    thickness = 1.0 / gGlobalGuiScale; // exactly one device pixel at any scale

    get_global_gui_scaled_mouse_coord(&coord);

    set_rgb_colour((tRgb)RGB_RED_7);
    render_rectangle(mainArea, (tRectangle){{0.0, coord.y}, {logicalW, thickness}});
    render_rectangle(mainArea, (tRectangle){{coord.x, 0.0}, {thickness, logicalH}});

    // Keep the readout on-screen when the cursor is near the right/top edge,
    // otherwise the one value you actually want to read is the one clipped away.
    snprintf(buff, sizeof(buff), "%.1f, %.1f", coord.x, coord.y);
    textX     = coord.x + 5.0;
    textY     = coord.y - 5.0;

    if (textX > logicalW - 90.0) {
        textX = coord.x - 90.0;
    }

    if (textY < textHeight) {
        textY = coord.y + textHeight + 5.0;
    }
    render_text(mainArea, (tRectangle){{textX, textY}, {BLANK_SIZE, textHeight}}, buff);
}
#endif

// THE FLOATING PANELS, ONCE. This list existed THREE TIMES — here for drawing, and twice in
// mouseHandle.c, once for clicks and once for keys — each copy filling a different column of the
// same struct and each carrying a comment warning that it had to agree with the others. It is the
// duplication the struct was introduced to remove, reintroduced one channel at a time.
//
// Sorted in place on every walk. That is not wasteful and it is not a cache: floating_panel_sort()
// orders by last-raised, which a click can change between one walk and the next, so asking again is
// the only way to be right.
static tFloatingPanelEntry gFloatingPanels[] = {
    {&gVirtualKeyboard.panel,   render_virtual_keyboard_panel, handle_virtual_keyboard_mouse, handle_virtual_keyboard_key, &gVirtualKeyboard.active  },
    {&gPatchAdjuster.panel,     render_patch_adjuster_panel,   handle_patch_adjuster_mouse,   handle_patch_adjuster_key,   &gPatchAdjuster.active    },
    {&gHelpPanel.panel,         render_help_panel,             handle_help_panel_mouse,       handle_help_panel_key,       &gHelpPanel.active        },
    {&gMutator.panel,           render_mutator_panel,          handle_mutator_mouse,          handle_mutator_key,          &gMutator.active          },
    {&gPatchSettingsEdit.panel, render_patch_settings_panel,   handle_patch_settings_mouse,   handle_patch_settings_key,   &gPatchSettingsEdit.active},
    {&gPerfSettingsEdit.panel,  render_perf_settings_panel,    handle_perf_settings_mouse,    handle_perf_settings_key,    &gPerfSettingsEdit.active },
    {&gPatchParamsEdit.panel,   render_patch_params_panel,     handle_patch_params_mouse,     handle_patch_params_key,     &gPatchParamsEdit.active  }
};

#define FLOATING_PANEL_COUNT    ((uint32_t)(sizeof(gFloatingPanels) / sizeof(gFloatingPanels[0])))

static void floating_panels_render(void) {
    // Panels stay off the canvas scrollbars, which run along the bottom and the right. Overlapping
    // the TOP bar is deliberately still allowed — a panel has to start somewhere, and the bar is not
    // something you scroll — but a panel lying over a scrollbar reads as a mistake rather than as a
    // panel in front. Set per frame so a window resize cannot leave it stale.
    floating_panel_set_bounds((tRectangle){{
                                               0.0, 0.0
                                           }, {
                                               (get_render_width() / gGlobalGuiScale) - SCROLLBAR_WIDTH,
                                               (get_render_height() / gGlobalGuiScale) - SCROLLBAR_WIDTH
                                           }
                              });

    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        gFloatingPanels[i].render();     // back to front, so the most recently clicked ends up on top
    }
}

// Reversed against the draw walk: sorted back-to-front for drawing, so front-to-back is the
// hit-test order. Fixed call order was wrong the moment two of them could overlap — whichever was
// tested first swallowed the press, even when it was the one underneath.
static bool floating_panels_mouse(tCoord coord, tMouseButton mouseButton) {
    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = FLOATING_PANEL_COUNT; i > 0; i--) {
        if (gFloatingPanels[i - 1].mouse(coord, mouseButton)) {
            return true;
        }
    }

    return false;
}

// Keys are ordered for the same reason clicks are: Escape has to close the panel you are LOOKING at.
// Fixed call order closed whichever handler came first — with the Help panel in front and the
// Virtual Keyboard behind it, Escape shut the keyboard.
static bool floating_panels_key(int key, int mods, int action) {
    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = FLOATING_PANEL_COUNT; i > 0; i--) {
        if (gFloatingPanels[i - 1].key(key, mods, action)) {
            return true;
        }
    }

    return false;
}

// THE POINTER IS OVER A PANEL — so the canvas underneath must not react to the motion.
//
// This is what the hover path needed and could not ask. cursor_pos() named the Mutator in an if and
// suppressed hover only for that one, so moving the pointer across Synth Settings (or any of the
// other five) ran the canvas hover detection underneath it: connectors the panel was covering lit
// up and the cable-hiding that goes with a connector hover triggered, over a panel. Reported
// 2026-08-20 against Synth Settings.
//
// Visibility is checked, not just the rectangle: a closed panel keeps its rect so it can reopen
// where it was left, and testing that alone would suppress hover over a strip of empty canvas.
bool floating_panels_under(tCoord coord) {
    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (  floating_panel_entry_visible(&gFloatingPanels[i])
           && floating_panel_contains(gFloatingPanels[i].panel, coord)) {
            return true;
        }
    }
    return false;
}

// A panel being MOVED owns the pointer until it is released. This was a fourth hand-written copy of
// the list — the draw, click and key copies are gone; this one had already lost the Mutator (which
// is fine, see below) and had a comment recording that the Help panel was once missed off it
// entirely, so it could be raised and closed but never moved.
//
// The Mutator is harmless to include even though cursor_pos() handles its move separately: that
// branch returns before this is reached whenever the Mutator is actually dragging, so the entry can
// only ever be a no-op here.
bool floating_panels_drag(tCoord coord) {
    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (floating_panel_drag(gFloatingPanels[i].panel, coord)) {
            return true;
        }
    }
    return false;
}

// The application's own popups, registered into SynthLib's ordering (synthlibPopups.h) so that this
// app's panels and the library's cannot disagree about who is in front.
//
// THE LAYERS ARE NOW THE WHOLE PIPELINE, not just the render order. Every entry below carries its
// mouse and key handlers, so this table is the answer to "who gets the click, and after whom" — a
// question that used to be answered by the ORDER OF THIRTEEN ifs in mouseHandle.c, restated a second
// time by the order of eight calls in render_frame(), with nothing anywhere able to check that the
// two agreed. They did not: the three panels below sat BELOW the floating panels when drawn and
// ABOVE them when clicked, so a floating panel lying over the Parameter Pages panel was drawn in
// front and took no clicks. That class of defect — paint order and hit order disagreeing — is the
// one this app has hit repeatedly (the context menu over the scrollbars, the VA module under the FX
// pane), and here it is now impossible to write down: ONE layer decides both.
//
// The numbers still reproduce exactly what the render calls did, which is what makes this a
// re-expression rather than a redesign, with one deliberate exception noted at patchNotes. Written
// relative to SynthLib's constants so the intent survives someone renumbering the library's layers.
static const tSynthLibPopup gAppPopups[] = {
    // ABOVE the context menu, because that is where it is DRAWN. Its clicks used to be offered after
    // the menu's, i.e. below — the one place where making input follow paint changes behaviour. A
    // menu raised over the notes editor is drawn underneath it, so it could previously be clicked
    // while invisible.
    {"patchNotes",     SYNTHLIB_POPUP_LAYER_CONTEXT_MENU + 10, false, NULL, render_patch_notes_edit,      NULL, handle_patch_notes_mouse,    NULL,                      NULL, NULL},
    {"bankBackup",     SYNTHLIB_POPUP_LAYER_CONTEXT_MENU + 20, false, NULL, render_bank_backup_progress,  NULL, NULL,                        NULL,                      NULL, NULL},
    {"bankRestore",    SYNTHLIB_POPUP_LAYER_CONTEXT_MENU + 30, false, NULL, render_bank_restore_progress, NULL, NULL,                        NULL,                      NULL, NULL},
    {"deviceBusy",     SYNTHLIB_POPUP_LAYER_BROWSERS + 10,     false, NULL, render_device_busy_overlay,   NULL, NULL,                        NULL,                      NULL, NULL},

    // The group, not the panels: their order among themselves is dynamic (they raise on click) and
    // belongs to floatingPanel.c. What is constant, and so belongs here, is that all of them sit
    // above the fixed panels below and below the context menu above.
    {"floatingPanels", SYNTHLIB_POPUP_LAYER_CONTEXT_MENU - 10, false, NULL, floating_panels_render,       NULL, floating_panels_mouse,       floating_panels_key,       NULL, NULL},

    // Drawn in this order, so ranked in it. These are what the menu bar's clicks had to stay behind.
    {"midiCcList",     SYNTHLIB_POPUP_LAYER_CONTEXT_MENU - 14, false, NULL, render_midi_cc_list_panel,    NULL, handle_midi_cc_list_mouse,   handle_midi_cc_list_key,   NULL, NULL},
    {"paramOverview",  SYNTHLIB_POPUP_LAYER_CONTEXT_MENU - 16, false, NULL, render_param_overview_panel,  NULL, handle_param_overview_mouse, handle_param_overview_key, NULL, NULL},
    {"paramPages",     SYNTHLIB_POPUP_LAYER_CONTEXT_MENU - 18, false, NULL, render_param_pages_panel,     NULL, handle_param_pages_mouse,    handle_param_pages_key,    NULL, NULL},
};

static void register_app_popups(void) {
    synthlib_popups_register(gAppPopups, (uint32_t)(sizeof(gAppPopups) / sizeof(gAppPopups[0])));
    synthlib_popups_set_menu_bar(gAppMenuBar, app_menu_bar_rect);
}

static void render_frame(void) {
    glClearColor(0.8, 0.8, 0.8, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // The sound engine reads the selected module's parameters from here. A redraw is exactly the
    // event it needs — every parameter change and every selection change causes one, whether it came
    // from the mouse or from the USB thread — so it needs no polling of its own and this costs
    // nothing when the engine is switched off.
    sound_engine_update_from_patch();

    clear_click_regions();

    // Draw each module pane in turn. render_modules()/render_cables() both read gLocation at their
    // top, so the Location is set around each pass in the same "mode rather than argument" style
    // the panes themselves use — which is why neither function needed a new parameter. gLocation is
    // put back to the focused pane's Location afterwards, since that is what every other reader in
    // the app means by it.
    split_view_apply();

    tLocation focusLocation = gLocation;
    uint32_t  focusPane     = split_view_focused_pane();

    param_overlay_begin_frame();   // hoisted out of render_modules(): one queue per FRAME, not per pane

    for (uint32_t pane = 0; pane < module_pane_count(); pane++) {
        set_module_pane(pane);
        gLocation = (tLocation)split_view_location_for_pane(pane);
        module_pane_clip_begin();
        render_modules();
        render_cables();
        // Inside the pane's own clip and transform. Drawing every pane's chips in one pass after
        // the loop put them all through the FOCUSED pane's transform with no scissor, so the Voice
        // Area's annotations landed over the FX Area.
        param_overlay_render_pane(pane);
        module_pane_clip_end();
    }

    set_module_pane(focusPane);
    gLocation = focusLocation;

    render_split_bar();

    if (gCableDrag.active == true) {
        tModule * module = get_module(gCableDrag.fromModuleKey);

        if (module != NULL) {
            tCableColour dragColour = cable_colour_for_connector_type(module->connector[gCableDrag.fromConnectorIndex].type);
            set_rgb_colour(gCableColourMap[dragColour]);
            render_cable_from_to(module->connector[gCableDrag.fromConnectorIndex], gCableDrag.toConnector, 4.0);
        }
    }
    render_top_bar();

    // The BAR itself stays here, ahead of the floating panels, because it is chrome they float above
    // — a panel is allowed to overlap it, and drawing the bar afterwards would put it over the panel
    // while the panel still took the click. Only its hover tick and its dropdown (which is the
    // context menu) are the coordinator's. See synthlibPopups.h.
    render_menu_bar(gAppMenuBar, app_menu_bar_rect());
    render_morph_groups();
    render_scrollbars();

    // ONE CALL, AND THE ORDER IS DATA. This used to be twelve calls whose sequence WAS the z-order —
    // the three fixed panels, the seven floating ones and SynthLib's own — correct, unstated, and
    // one careless insertion away from being wrong. Every one of them is now a row in gAppPopups
    // above, ranked by the same layer that decides which of them gets the click. See
    // synthlibPopups.h.
    synthlib_popups_render();

#ifdef ENABLE_MOUSE_CROSSHAIR
    render_mouse_crosshair();     // TEMPORARY debug aid (F9) — above even the modal, so it is never hidden
#endif

    glfwSwapBuffers((GLFWwindow *)synthlib_window());
}

// ── Backdoor test-control channel ───────────────────────────────────────────
// A way to drive AND independently verify the running app — load a patch,
// select a slot, dump the module list, or capture a screenshot — without a
// real mouse click or a reliable headless paint event. Ported from SynthEdit's
// proven mechanism (SynthEdit/src/graphics.cpp), adapted to G2's domain.
//
// GATED behind the G2_EDIT_BACKDOOR environment variable: unset (the owner's
// normal double-click launch) => backdoor completely inert AND the idle loop
// keeps glfwWaitEvents()'s full sleep. Set (a test launch from a shell) =>
// the idle loop polls at 10 Hz and a command file is honoured each tick. Unlike
// SynthEdit (sandboxed, needs its container tmp dir) G2-Edit has no App Sandbox,
// so plain /tmp works. Command surface is deliberately narrow and does nothing a
// real mouse click couldn't already do.
//
// Command file (/tmp/g2edit_cmd.txt): one command per file, first line only,
// "<COMMAND> <arg>". Result ("OK\n"/"ERROR: ...\n", or DUMP's own text) is
// written to /tmp/g2edit_result.txt and the command file is deleted, so a
// caller polls for the command file's disappearance to know it's done.
//   LOADFILE <path>   — read_file_into_memory_and_process() (works offline)
//   SLOT <0-3|A-D>    — select the slot the canvas renders
//   DUMP              — current slot + every module: type, name, location, col/row
//   PARAMDUMP         — the same modules with their variation-0 PARAMETER and MODE values, plus the
//                       parameter count the patch declared against the one our table gives
//   MENU <bar>[/<item>[/<sub>]] — run a menu item by label (leading substring, case-insensitive,
//                       '/' separated); omit the last level to LIST what that level contains
//   SELECT <VA|FX> <n> — select one module by index; SELECT NONE clears
//   SNDSTATUS         — what the sound engine's status line currently reads
//   SNDDUMP           — the resolved chain, the parameters read, and the peak level since last read
//   NOTE <n>|OFF      — play/release a note on the sound engine (LOCAL engine, not the G2)
//   DEVSET <VA|FX> <index> <param> <value> — as SET, but SENT TO THE G2. This is what lets the
//                       measurement harness step one parameter on the hardware while its audio output
//                       is recorded; SET stays local-only so a rendering test cannot write to a
//                       connected synth by accident.
//   DEVMODE <VA|FX> <index> <mode> <value> — a MODE write to the G2 (the drop-down selectors: the
//                       Reverb's room size, a filter's slope, an oscillator's waveform). Modes travel
//                       on their own wire command, so DEVSET cannot reach them.
//   DEVKNOB <knob 0-119> <VA|FX> <index> <param> — assign a parameter to a patch knob, on the G2 as
//                       well as locally. Needed before the synth's own display can be asked what a
//                       dial reads: an unassigned parameter has nowhere to show itself on the panel.
//   DEVNOTE <note> <vel> on|off — a Virtual Keyboard note to the G2, for a patch that needs a gate
//                       rather than a free-running clock (envelope times, for instance)
//   SAVEFILE <path>   — write the current slot to a path (no save panel)
//   SCREENSHOT <path> — synchronous render_frame() then glReadPixels + PNG
//   SCROLL <x> <y>    — scroll the canvas, each 0.0-1.0 of that axis's full travel
//   ZOOM <factor>     — canvas zoom, same 0.25-2.0 range Cmd +/- walks through
//
// SCROLL and ZOOM exist because a synthetic drag doesn't reach the app at all — neither the
// scrollbar thumb nor a dial responds to one — so without them a scripted check can only ever see
// the modules that happen to be on screen at the default zoom.
static bool backdoor_enabled(void) {
    static int cached = -1;

    if (cached < 0) {
        const char * v = getenv("G2_EDIT_BACKDOOR");
        cached = (v != NULL && v[0] != '\0') ? 1 : 0;
    }
    return cached == 1;
}

static const char * backdoor_cmd_path(void) {
    return "/tmp/g2edit_cmd.txt";
}

static const char * backdoor_result_path(void) {
    return "/tmp/g2edit_result.txt";
}

// Case-insensitive "does this label contain that text", for the MENU command's label matching.
static bool label_contains(const char * label, const char * wanted) {
    size_t wantedLength = strlen(wanted);
    size_t labelLength  = strlen(label);
    size_t at           = 0;

    if (wantedLength == 0) {
        return false;
    }

    if (wantedLength > labelLength) {
        return false;
    }

    for (at = 0; at <= (labelLength - wantedLength); at++) {
        if (strncasecmp(label + at, wanted, wantedLength) == 0) {
            return true;
        }
    }

    return false;
}

static void backdoor_write_result(const char * text) {
    FILE * f = fopen(backdoor_result_path(), "w");

    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void backdoor_screenshot(const char * path) {
    render_frame(); // synchronous, so the capture always reflects the most recent LOADFILE/SLOT command, not a stale frame

    int       w      = get_render_width();
    int       h      = get_render_height();

    if ((w <= 0) || (h <= 0)) {
        backdoor_write_result("ERROR: zero-size framebuffer\n");
        return;
    }
    uint8_t * pixels = (uint8_t *)malloc((size_t)w * (size_t)h * 3);

    if (!pixels) {
        backdoor_write_result("ERROR: out of memory\n");
        return;
    }
    // Tightly-packed rows (w*3 bytes). Without this, glReadPixels' default
    // GL_PACK_ALIGNMENT of 4 pads each row up to a 4-byte multiple whenever w*3
    // isn't already one (i.e. any width not a multiple of 4) — which both shears
    // the saved PNG (row stride mismatch vs stbi's w*3) AND overruns the w*h*3
    // buffer. Only bit us at odd window sizes; Retina captures were multiples of 4.
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
    stbi_flip_vertically_on_write(1); // GL origin is bottom-left; PNGs are top-down

    int       ok     = stbi_write_png(path, w, h, 3, pixels, w * 3);

    free(pixels);
    backdoor_write_result(ok ? "OK\n" : "ERROR: stbi_write_png failed\n");
}

static void backdoor_dump_state(char * out, size_t outMax) {
    size_t         used       = 0;
    const uint32_t locs[]     = {(uint32_t)locationVa, (uint32_t)locationFx};
    const char *   locNames[] = {"VA", "FX"};

    used += (size_t)snprintf(out + used, outMax - used, "OK\nslot=%u\n", (unsigned)gSlot);

    for (uint32_t l = 0; (l < 2) && (used < outMax); l++) {
        for (uint32_t index = 0; (index < MAX_NUM_MODULES) && (used < outMax); index++) {
            tModule * module = get_module_slot(gSlot, locs[l], index);

            if (module == NULL || module->type == 0) {
                continue; // type 0 == empty slot in the sparse per-index store
            }
            used += (size_t)snprintf(out + used, outMax - used,
                                     "loc=%s index=%u type=%u name=\"%s\" col=%u row=%u\n",
                                     locNames[l], (unsigned)index, (unsigned)module->type,
                                     module->name, (unsigned)module->column, (unsigned)module->row);
        }
    }

    // Cables too, so a caller can verify a cable edit — and its undo — without a screenshot.
    // Emitted sorted-by-nothing, i.e. in database order, which a delete-and-recreate reshuffles;
    // compare these as a SET rather than line-by-line.
    for (uint32_t l = 0; (l < 2) && (used < outMax); l++) {
        for (uint32_t index = 0; (index < MAX_NUM_CABLES) && (used < outMax); index++) {
            tCable * cable = get_cable_slot(gSlot, locs[l], index);

            if ((cable == NULL) || !cable->active) {
                continue;
            }
            used += (size_t)snprintf(out + used, outMax - used,
                                     "cable loc=%s from=%u:%u link=%u to=%u:%u colour=%u\n",
                                     locNames[l],
                                     (unsigned)cable->key.moduleFromIndex, (unsigned)cable->key.connectorFromIoCount,
                                     (unsigned)cable->key.linkType,
                                     (unsigned)cable->key.moduleToIndex, (unsigned)cable->key.connectorToIoCount,
                                     (unsigned)cable->colour);
        }
    }
}

// PARAMDUMP — every active module in the current slot with its VARIATION 0 parameter values and its
// mode values. DUMP above reports only STRUCTURE (which modules, which cables); this reports
// CONTENTS, which is what auditing paramLocationList's defaultValue column against a reference patch
// needs.
//
// Prints the count the PATCH declared (module->actualParamCount) alongside the count our own table
// gives, so the same dump doubles as a param-count check on any device-authored file.
//
// Written straight to the result file rather than composed in a buffer the way backdoor_dump_state()
// is: a full patch runs to tens of modules by tens of parameters, which overruns any fixed buffer
// worth putting on the stack.
static void backdoor_param_dump(void) {
    FILE *         file       = fopen(backdoor_result_path(), "w");

    if (file == NULL) {
        return;
    }
    const uint32_t locs[]     = {(uint32_t)locationVa, (uint32_t)locationFx};
    const char *   locNames[] = {"VA", "FX"};

    fprintf(file, "OK\nslot=%u\n", (unsigned)gSlot);

    for (uint32_t l = 0; l < 2; l++) {
        for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
            tModule * module = get_module_slot(gSlot, locs[l], index);

            if ((module == NULL) || (module->type == 0)) {
                continue; // type 0 == empty slot in the sparse per-index store
            }
            fprintf(file, "module loc=%s index=%u type=%u name=\"%s\" filecount=%u tablecount=%u params:",
                    locNames[l], (unsigned)index, (unsigned)module->type, module->name,
                    (unsigned)module->actualParamCount, (unsigned)module_param_count(module->type));

            for (uint32_t p = 0; (p < module->actualParamCount) && (p < MAX_NUM_PARAMETERS); p++) {
                fprintf(file, " %u", (unsigned)module->param[0][p].value);
            }

            fprintf(file, "\nmodes loc=%s index=%u count=%u:", locNames[l], (unsigned)index, (unsigned)module->modeCount);

            for (uint32_t m = 0; (m < module->modeCount) && (m < MAX_NUM_MODES); m++) {
                fprintf(file, " %u", (unsigned)module->mode[m].value);
            }

            fprintf(file, "\n");
        }
    }

    fclose(file);
}

// A cable end is addressed by its I/O index — "output 2", "input 0" — counting only connectors of
// that direction. That is how the protocol expresses it, how tCableKey stores it, and how the DUMP
// command above prints it. A module's connector array is in declaration order with both directions
// interleaved, so turning one into the other is a walk. -1 if the module has no such connector.
static int32_t backdoor_connector_for_io_index(tModule * module, bool wantOutput, uint32_t ioIndex) {
    uint32_t count = module_connector_count(module->type);
    uint32_t seen  = 0;

    for (uint32_t i = 0; i < count; i++) {
        if ((module->connector[i].dir == connectorDirOut) == wantOutput) {
            if (seen == ioIndex) {
                return (int32_t)i;
            }
            seen++;
        }
    }

    return -1;
}

// Shared argument parsing for CABLE and DELCABLE: "<VA|FX> <from>:<out> <to>:<in> [link=<0|1>]",
// deliberately the same shape DUMP prints, so a dumped cable can be pasted straight back as a
// command. link defaults to 1 (the from-end is an output); 0 is a fan-out from an input connector.
static bool backdoor_parse_cable(const char * arg, tCableKey * key, char * err, size_t errMax) {
    char         loc[8]    = {0};
    uint32_t     fromIndex = 0;
    uint32_t     fromIo    = 0;
    uint32_t     toIndex   = 0;
    uint32_t     toIo      = 0;
    const char * linkText  = NULL;
    uint32_t     link      = (uint32_t)cableLinkTypeFromOutput;

    if (sscanf(arg, "%7s %u:%u %u:%u", loc, &fromIndex, &fromIo, &toIndex, &toIo) != 5) {
        snprintf(err, errMax, "ERROR: expected '<VA|FX> <from>:<out> <to>:<in> [link=<0|1>]'\n");
        return false;
    }
    linkText                  = strstr(arg, "link=");

    if ((linkText != NULL) && (sscanf(linkText, "link=%u", &link) != 1)) {
        snprintf(err, errMax, "ERROR: link must be 0 or 1\n");
        return false;
    }

    if (link > (uint32_t)cableLinkTypeFromOutput) {
        snprintf(err, errMax, "ERROR: link must be 0 or 1\n");
        return false;
    }
    key->slot                 = gSlot;
    key->location             = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
    key->moduleFromIndex      = fromIndex;
    key->connectorFromIoCount = fromIo;
    key->linkType             = link;
    key->moduleToIndex        = toIndex;
    key->connectorToIoCount   = toIo;
    return true;
}

// An input connector takes at most one incoming cable — the same invariant cable-drag creation
// enforces before it commits. Checked here too, because the device silently keeps whichever it
// likes when told otherwise.
// Is this connector already serving as the input end of some cable? An input takes exactly one
// cable, so this is what stops a scripted CABLE from stacking a second one on top of an existing
// connection - which looks like nothing at all on screen, because the two are drawn along the same
// path, and only shows itself when you pull one off and the sound stays.
//
// BOTH ENDS OF A cableLinkTypeFromInput CABLE ARE INPUTS (see cableChain.h): that link type is the
// G2's input-to-input daisy chain, so its from-end is an input just as much as its to-end is. This
// used to test the to-end alone, so an input already spoken for as the FROM-end of such a chain
// read as free and got a second cable.
static bool backdoor_connector_is_input_end(const tCableKey * cableKey, uint32_t moduleIndex, uint32_t ioCount) {
    if ((cableKey->moduleToIndex == moduleIndex) && (cableKey->connectorToIoCount == ioCount)) {
        return true;
    }
    return (cableKey->linkType == (uint32_t)cableLinkTypeFromInput)
           && (cableKey->moduleFromIndex == moduleIndex)
           && (cableKey->connectorFromIoCount == ioCount);
}

static bool backdoor_input_is_taken(const tCableKey * key) {
    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(key->slot, key->location, i);

        if ((cable == NULL) || !cable->active) {
            continue;
        }

        // The new cable's to-end is always an input; its from-end is one too when it is itself an
        // input-to-input link, and either would be a second cable on an already-occupied input.
        if (backdoor_connector_is_input_end(&cable->key, key->moduleToIndex, key->connectorToIoCount)) {
            return true;
        }

        if (  (key->linkType == (uint32_t)cableLinkTypeFromInput)
           && backdoor_connector_is_input_end(&cable->key, key->moduleFromIndex, key->connectorFromIoCount)) {
            return true;
        }
    }

    return false;
}

static void backdoor_dispatch(const char * cmd, const char * arg) {
    if (strcmp(cmd, "LOADFILE") == 0) {
        if (arg[0] == '\0') {
            backdoor_write_result("ERROR: expected 'LOADFILE <path>'\n");
            return;
        }
        read_file_into_memory_and_process(arg);
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "SLOT") == 0) {
        uint32_t slot = 0;

        if (arg[0] >= 'A' && arg[0] <= 'D') {
            slot = (uint32_t)(arg[0] - 'A');
        } else if (arg[0] >= 'a' && arg[0] <= 'd') {
            slot = (uint32_t)(arg[0] - 'a');
        } else if (sscanf(arg, "%u", &slot) != 1 || slot > 3) {
            backdoor_write_result("ERROR: expected 'SLOT <0-3|A-D>'\n");
            return;
        }
        gSlot                 = slot;
        gPatchParamsEdit.slot = slot; // patch-params panel tracks its own slot copy — keep both in step
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "NEWPATCH") == 0) {
        // Clears the current slot's canvas (modules + cables). Offline layout
        // scratchpad — a real device New Patch would go through eMsgCmdNewPatch.
        database_delete_modules_by_slot(gSlot);
        database_delete_cables_by_slot(gSlot);
        gLocation = (uint32_t)locationVa;
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "ADDMODULE") == 0) {
        // ADDMODULE <name> [col] [row] — name matches gModuleProperties[].name
        // (e.g. "Mix4-1C"). Added to the Voice area at col/row (default 0,0).
        char        name[64] = {0};
        uint32_t    col      = 0;
        uint32_t    row      = 0;

        if (sscanf(arg, "%63s %u %u", name, &col, &row) < 1) {
            backdoor_write_result("ERROR: expected 'ADDMODULE <name> [col] [row]'\n");
            return;
        }
        tModuleType found    = (tModuleType)0;

        for (uint32_t t = 1; t < (uint32_t)moduleTypeMax; t++) {
            if (strcmp(gModuleProperties[t].name, name) == 0) {
                found = (tModuleType)t;
                break;
            }
        }

        if (found == (tModuleType)0) {
            char msg[128];

            snprintf(msg, sizeof(msg), "ERROR: no module named '%s'\n", name);
            backdoor_write_result(msg);
            return;
        }
        gLocation = (uint32_t)locationVa;

        int32_t     idx      = create_module_at(found, col, row, false); // false = local-only; the backdoor never writes to the device

        synthlib_request_redraw();
        backdoor_write_result((idx < 0) ? "ERROR: location full\n" : "OK\n");
    } else if ((strcmp(cmd, "CABLE") == 0) || (strcmp(cmd, "DELCABLE") == 0)) {
        // CABLE / DELCABLE <VA|FX> <from>:<out> <to>:<in> [link=<0|1>]
        //
        // LOCAL-ONLY, like ADDMODULE and SET: it edits the database and nothing else. Follow a run of
        // these with PUSH to send the whole slot to the device as ONE versioned command. Sending each
        // edit as it is made would race the G2's asynchronous patch-version notification and lose
        // some of them silently — see the note above send_whole_patch() in menus.c.
        bool      removing      = (cmd[0] == 'D');
        tCableKey key           = {0};
        char      msg[160]      = {0};

        if (!backdoor_parse_cable(arg, &key, msg, sizeof(msg))) {
            backdoor_write_result(msg);
            return;
        }

        if (removing) {
            if (get_cable(key) == NULL) {
                backdoor_write_result("ERROR: no such cable\n");
                return;
            }
            delete_cable(key);
            synthlib_request_redraw();
            backdoor_write_result("OK\n");
            return;
        }
        tModule * fromModule    = get_module_slot(key.slot, key.location, key.moduleFromIndex);
        tModule * toModule      = get_module_slot(key.slot, key.location, key.moduleToIndex);

        if ((fromModule == NULL) || (fromModule->type == 0) || (toModule == NULL) || (toModule->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }
        int32_t   fromConnector = backdoor_connector_for_io_index(fromModule, key.linkType == (uint32_t)cableLinkTypeFromOutput, key.connectorFromIoCount);
        int32_t   toConnector   = backdoor_connector_for_io_index(toModule, false, key.connectorToIoCount);

        if ((fromConnector < 0) || (toConnector < 0)) {
            backdoor_write_result("ERROR: connector index out of range for that module type\n");
            return;
        }

        if (backdoor_input_is_taken(&key)) {
            backdoor_write_result("ERROR: that input already has a cable\n");
            return;
        }
        tCable    cable         = {0};

        // The cable inherits the from-connector's CURRENT colour, up-rate promotion included — the
        // same rule cable-drag creation follows, so a scripted patch looks like a drawn one.
        cable.colour = (uint32_t)cable_colour_for_connector_type(
            effective_connector_type(fromModule->connector[fromConnector].type, fromModule->upRate));
        write_cable(key, &cable);
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "PUSH") == 0) {
        // Sends the current slot to the device as one whole-patch write, which is what makes a run of
        // local CABLE/ADDMODULE/SET edits real. One command, one patch version, nothing to race.
        tMessageContent msg = {0};

        msg.cmd  = eMsgCmdWritePatch;
        msg.slot = gSlot;
        msg_send(&gToUsbThread, &msg);
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "SET") == 0) {
        // SET <VA|FX> <index> <param> <value> — set a param's value in the
        // current slot, LOCAL-ONLY (no device write); for inspecting how a
        // param renders across its range.
        char      loc[8]   = {0};
        uint32_t  index    = 0;
        uint32_t  param    = 0;
        uint32_t  value    = 0;

        if (sscanf(arg, "%7s %u %u %u", loc, &index, &param, &value) != 4) {
            backdoor_write_result("ERROR: expected 'SET <VA|FX> <index> <param> <value>'\n");
            return;
        }
        uint32_t  location = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModule * module   = get_module_slot(gSlot, location, index);

        if ((module == NULL) || (module->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }

        if (param >= MAX_NUM_PARAMETERS) {
            backdoor_write_result("ERROR: param index out of range\n");
            return;
        }
        module->param[gPatchDescr[gSlot].activeVariation][param].value = value;
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "DEVSET") == 0) {
        // DEVSET <VA|FX> <index> <param> <value> — like SET, but SENDS THE CHANGE TO THE G2 as a dial
        // drag would, instead of only touching the local copy.
        //
        // This exists for the measurement harness: characterising a module means stepping one of its
        // parameters over its range while recording the device's audio output, and that only works if
        // the hardware actually follows. SET stays local-only for its own purpose (seeing how a value
        // RENDERS), and the two are deliberately separate commands so a rendering test can never
        // write to a connected synth by accident.
        //
        // Nothing here is destructive: this is a live parameter edit, exactly what the canvas does on
        // every drag, and it does not store to flash.
        char      loc[8]     = {0};
        uint32_t  index      = 0;
        uint32_t  param      = 0;
        uint32_t  value      = 0;

        if (sscanf(arg, "%7s %u %u %u", loc, &index, &param, &value) != 4) {
            backdoor_write_result("ERROR: expected 'DEVSET <VA|FX> <index> <param> <value>'\n");
            return;
        }
        uint32_t  location   = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModule * module     = get_module_slot(gSlot, location, index);

        if ((module == NULL) || (module->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }
        // VALIDATED AGAINST THE MODULE'S OWN PARAM COUNT, not against the array bound. MAX_NUM_PARAMETERS
        // is the size of the store, not the number this module has, and a write past the real count goes
        // out on the wire, gets dropped by the G2, and reports OK — the local copy has already changed,
        // so nothing anywhere says the device disagreed.
        //
        // THAT COST THREE MEASUREMENT RUNS. The Reverb's Small/Medium/Large/Hall selector is a MODE, and
        // a sweep that drove it as `DEVSET <index> 4` produced three files of the same room with no
        // complaint from anything. The lengths only came out identical because they genuinely were.
        uint32_t  paramCount = module_param_count(module->type);

        if (param >= paramCount) {
            char     msg[160];
            uint32_t modeCount = module->modeCount;

            snprintf(msg, sizeof(msg), "ERROR: param %u out of range (module has %u)%s\n",
                     (unsigned)param, (unsigned)paramCount,
                     (modeCount > 0) ? " — a drop-down selector is a MODE: try DEVMODE" : "");
            backdoor_write_result(msg);
            return;
        }
        uint32_t  variation  = gPatchDescr[gSlot].activeVariation;

        module->param[variation][param].value = (uint8_t)value;
        send_param_value(gSlot, module->key, param, variation, value);
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "DEVKNOB") == 0) {
        // DEVKNOB <knob 0-119> <VA|FX> <index> <param> — assign a parameter to one of the patch's
        // knobs, on the G2 as well as locally. The same thing the canvas's Assign Knob menu does.
        //
        // This is what makes a hardware reading possible at all: a parameter the panel has not been
        // pointed at cannot be shown on the synth's display, so a question of the form "what does
        // this dial actually read" needs the assignment before it needs the value.
        //
        // Knob numbering is the 0-119 the patch stores: 24 to a page, 8 to a bank within it, so
        // knob 0 is page 1 bank A position 1 — the first knob of the first page.
        char            loc[8]    = {0};
        uint32_t        knobIndex = 0;
        uint32_t        index     = 0;
        uint32_t        param     = 0;

        if (sscanf(arg, "%u %7s %u %u", &knobIndex, loc, &index, &param) != 4) {
            backdoor_write_result("ERROR: expected 'DEVKNOB <knob 0-119> <VA|FX> <index> <param>'\n");
            return;
        }

        if (knobIndex >= MAX_NUM_KNOBS) {
            backdoor_write_result("ERROR: knob index out of range (0-119)\n");
            return;
        }
        uint32_t        location  = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModule *       module    = get_module_slot(gSlot, location, index);

        if ((module == NULL) || (module->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }

        if (param >= module_param_count(module->type)) {
            backdoor_write_result("ERROR: param out of range for that module type\n");
            return;
        }
        tMessageContent msg       = {0};

        // Free the knob first if something is already on it, exactly as the menu does - the G2 keeps
        // one parameter per knob and a bare assign over an occupied one is not the way to replace it.
        if (gKnobArray[gSlot].knob[knobIndex].assigned) {
            msg.cmd                        = eMsgCmdDeassignKnob;
            msg.slot                       = gSlot;
            msg.knobDeassignData.knobIndex = knobIndex;
            msg_send(&gToUsbThread, &msg);
            memset(&msg, 0, sizeof(msg));
        }
        gKnobArray[gSlot].knob[knobIndex].assigned    = true;
        gKnobArray[gSlot].knob[knobIndex].location    = location;
        gKnobArray[gSlot].knob[knobIndex].moduleIndex = index;
        gKnobArray[gSlot].knob[knobIndex].isLed       = 0;
        gKnobArray[gSlot].knob[knobIndex].paramIndex  = param;

        msg.cmd                                       = eMsgCmdAssignKnob;
        msg.slot                                      = gSlot;
        msg.knobAssignData.moduleKey                  = module->key;
        msg.knobAssignData.paramIndex                 = param;
        msg.knobAssignData.knobIndex                  = knobIndex;
        msg_send(&gToUsbThread, &msg);

        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "DEVMODE") == 0) {
        // DEVMODE <VA|FX> <index> <mode> <value> — a MODE write to the G2, the companion to DEVSET.
        //
        // Modes are the drop-down selectors, and they travel on their own wire command rather than as
        // parameters: the Reverb's Small/Medium/Large/Hall is a mode, as are a filter's slope and an
        // oscillator's waveform. Measuring across those settings is exactly what the harness is for, so
        // without this the most valuable sweep of all — the four reverb room sizes, which are data that
        // exists nowhere else — could not be driven at all.
        char      loc[8]   = {0};
        uint32_t  index    = 0;
        uint32_t  mode     = 0;
        uint32_t  value    = 0;

        if (sscanf(arg, "%7s %u %u %u", loc, &index, &mode, &value) != 4) {
            backdoor_write_result("ERROR: expected 'DEVMODE <VA|FX> <index> <mode> <value>'\n");
            return;
        }
        uint32_t  location = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModule * module   = get_module_slot(gSlot, location, index);

        if ((module == NULL) || (module->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }

        if (mode >= module->modeCount) {
            char msg[96];

            snprintf(msg, sizeof(msg), "ERROR: mode %u out of range (module has %u)\n",
                     (unsigned)mode, (unsigned)module->modeCount);
            backdoor_write_result(msg);
            return;
        }
        module->mode[mode].value = (uint8_t)value;
        send_mode_value(gSlot, module->key, mode, value);
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "DEVNOTE") == 0) {
        // DEVNOTE <note> <velocity> on|off — a Virtual Keyboard note to the DEVICE, for exciting a
        // patch that needs a gate rather than a free-running clock (envelope times, for instance).
        char            state[8] = {0};
        uint32_t        note     = 0;
        uint32_t        velocity = 0;

        if (sscanf(arg, "%u %u %7s", &note, &velocity, state) != 3) {
            backdoor_write_result("ERROR: expected 'DEVNOTE <note> <velocity> on|off'\n");
            return;
        }

        if ((note > 127) || (velocity > 127)) {
            backdoor_write_result("ERROR: note and velocity are 0-127\n");
            return;
        }
        tMessageContent msg      = {0};

        msg.cmd                   = eMsgCmdPlayNote;
        msg.playNoteData.note     = note;
        msg.playNoteData.velocity = velocity;
        msg.playNoteData.on       = (state[0] == 'o') && (state[1] == 'n');
        msg_send(&gToUsbThread, &msg);
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "DUMP") == 0) {
        char dump[16384];

        backdoor_dump_state(dump, sizeof(dump));
        backdoor_write_result(dump);
    } else if (strcmp(cmd, "PARAMDUMP") == 0) {
        backdoor_param_dump();
    } else if (strcmp(cmd, "MENU") == 0) {
        // MENU <bar>[/<item>[/<subitem>]] — runs a menu item by label without going near the mouse.
        // Labels are matched case-insensitively on a leading substring and separated by '/', so
        // 'MENU Exp/Audio Device/QU-24' reaches into a flyout. Any trailing level omitted, the
        // deepest menu reached is LISTED instead of clicked, which is how a test discovers what is
        // there. Driving these by screen coordinates meant re-deriving them whenever a window moved.
        char        part[3][64] = {0};
        uint32_t    partCount   = 0;
        uint32_t    b           = 0;
        tMenuItem * items       = NULL;

        {
            const char * p = arg;

            while ((partCount < 3) && (*p != '\0')) {
                const char * slash  = strchr(p, '/');
                size_t       length = (slash != NULL) ? (size_t)(slash - p) : strlen(p);

                while ((length > 0) && (*p == ' ')) {
                    p++;
                    length--;
                }

                while ((length > 0) && (p[length - 1] == ' ')) {
                    length--;
                }

                if (length >= sizeof(part[0])) {
                    length = sizeof(part[0]) - 1;
                }
                memcpy(part[partCount], p, length);
                part[partCount][length] = '\0';
                partCount++;

                if (slash == NULL) {
                    break;
                }
                p                       = slash + 1;
            }
        }

        if (partCount == 0) {
            backdoor_write_result("ERROR: expected 'MENU <bar>[/<item>[/<subitem>]]'\n");
            return;
        }

        for (b = 0; gAppMenuBar[b].label != NULL; b++) {
            if (label_contains(gAppMenuBar[b].label, part[0]) == true) {
                break;
            }
        }

        if (gAppMenuBar[b].label == NULL) {
            backdoor_write_result("ERROR: no such menu\n");
            return;
        }
        // Populates gContextMenu with the items that menu would show right now, which is what makes
        // state-dependent labels ("Disable Sound Engine") matchable.
        gAppMenuBar[b].open((tCoord){0.0, 0.0});
        items = (gContextMenu.depth > 0) ? gContextMenu.frame[0].items : NULL;

        {
            uint32_t level = 1;

            // Walk down through the named levels. Every level but the last must be a flyout.
            while ((level < partCount) && (items != NULL)) {
                uint32_t i     = 0;
                bool     found = false;

                for (i = 0; items[i].label != NULL; i++) {
                    // Matched ANYWHERE in the label, not just at the front: menu labels carry a
                    // leading "* " marker for the current selection, so a leading-substring match
                    // could never name the thing being selected.
                    if (label_contains(items[i].label, part[level]) == false) {
                        continue;
                    }
                    found = true;

                    if (level == (partCount - 1)) {
                        // The deepest named level: click it, unless it is itself a flyout, in which
                        // case descend so the listing below shows what it contains.
                        if (items[i].subMenu != NULL) {
                            items = items[i].subMenu;
                            break;
                        }
                        {
                            void (*action)(int index) = items[i].action;

                            // action() callbacks read gContextMenu.items[index].param, so point that
                            // at the array the item lives in — the same thing a real click does.
                            gContextMenu.items = items;

                            if (action == NULL) {
                                close_context_menu();
                                backdoor_write_result("ERROR: item is disabled\n");
                                return;
                            }
                            action((int)i);
                            close_context_menu();
                            synthlib_request_redraw();
                            backdoor_write_result("OK\n");
                            return;
                        }
                    }

                    if (items[i].subMenu == NULL) {
                        close_context_menu();
                        backdoor_write_result("ERROR: that item has no submenu\n");
                        return;
                    }
                    items = items[i].subMenu;
                    break;
                }

                if (found == false) {
                    close_context_menu();
                    backdoor_write_result("ERROR: no such item\n");
                    return;
                }
                level++;
            }
        }

        // Nothing left to click: list what the level we reached contains.
        {
            char     list[2048] = {0};
            size_t   used       = 0;
            uint32_t i          = 0;

            used += (size_t)snprintf(list + used, sizeof(list) - used, "OK\n");

            for (i = 0; (items != NULL) && (items[i].label != NULL) && (used < sizeof(list)); i++) {
                used += (size_t)snprintf(list + used, sizeof(list) - used, "%s%s\n",
                                         items[i].label, (items[i].subMenu != NULL) ? " >" : "");
            }

            close_context_menu();
            backdoor_write_result(list);
        }
    } else if (strcmp(cmd, "SELECT") == 0) {
        // SELECT <VA|FX> <index>, or SELECT NONE — the engine keys off the selection, and clicking a
        // module's header strip by coordinate was the single most error-prone step in driving it.
        char     locName[8] = {0};
        uint32_t index      = 0;

        if (strncasecmp(arg, "NONE", 4) == 0) {
            selection_clear();
            synthlib_request_redraw();
            backdoor_write_result("OK\n");
            return;
        }

        // SELECT with no argument reports what is selected, so a test can check the selection
        // rather than infer it from a screenshot. Without this the clear-on-switch behaviour is
        // invisible to anything driving the app from outside.
        if ((arg[0] == '\0') || (arg[0] == '?')) {
            char report[512] = {0};
            int  used        = snprintf(report, sizeof(report), "OK\ncount=%u\n", gSelection.count);

            for (uint32_t si = 0; (si < gSelection.count) && (used < (int)sizeof(report) - 48); si++) {
                used += snprintf(report + used, sizeof(report) - (size_t)used,
                                 "  slot=%u loc=%s index=%u\n",
                                 gSelection.keys[si].slot,
                                 (gSelection.keys[si].location == locationVa) ? "VA" : "FX",
                                 gSelection.keys[si].index);
            }

            backdoor_write_result(report);
            return;
        }

        if (sscanf(arg, "%7s %u", locName, &index) != 2) {
            backdoor_write_result("ERROR: expected 'SELECT <VA|FX> <index>' or 'SELECT NONE'\n");
            return;
        }
        {
            uint32_t  location = (strncasecmp(locName, "FX", 2) == 0) ? (uint32_t)locationFx : (uint32_t)locationVa;
            tModule * module   = get_module_slot(gSlot, location, index);

            if ((module == NULL) || (module->type == 0)) {
                backdoor_write_result("ERROR: no module at that index\n");
                return;
            }
            selection_set_single((tModuleKey){gSlot, location, index});
            synthlib_request_redraw();
            backdoor_write_result("OK\n");
        }
    } else if (strcmp(cmd, "SAVEFILE") == 0) {
        // SAVEFILE <path> — writes the current slot straight to a path, no save panel involved.
        // Driving the native panel with synthetic keystrokes is how a test patch got overwritten;
        // this exists so a round-trip can be checked without going anywhere near it.
        tMessageContent msg = {0};

        if ((arg == NULL) || (arg[0] == '\0')) {
            backdoor_write_result("ERROR: expected 'SAVEFILE <path>'\n");
            return;
        }
        msg.cmd                = eMsgCmdSavePatchFile;
        msg.patchFileData.slot = gSlot;
        strncpy(msg.patchFileData.filePath, arg, sizeof(msg.patchFileData.filePath) - 1);
        msg_send(&gToUsbThread, &msg);
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "NOTE") == 0) {
        // NOTE <midi note> plays, NOTE OFF releases. The last thing that needed a mouse to test the
        // sound engine end to end.
        int32_t note = 0;

        if (strncasecmp(arg, "OFF", 3) == 0) {
            sound_engine_note(-1, false);
            backdoor_write_result("OK\n");
            return;
        }

        if (sscanf(arg, "%d", &note) != 1) {
            backdoor_write_result("ERROR: expected 'NOTE <0-127>' or 'NOTE OFF'\n");
            return;
        }
        sound_engine_note(note, true);
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "SNDDUMP") == 0) {
        char text[8400] = {0};

        snprintf(text, sizeof(text), "OK\n%s", sound_engine_debug_text());
        backdoor_write_result(text);
    } else if (strcmp(cmd, "SNDSTATUS") == 0) {
        // Reads back what the Experimental menu would show, so a test can assert on why the engine
        // is or is not making a sound without taking a screenshot of a menu.
        char text[160] = {0};

        snprintf(text, sizeof(text), "OK\n%s\n", sound_engine_status_text());
        backdoor_write_result(text);
    } else if (strcmp(cmd, "SCROLL") == 0) {
        double xFraction = 0.0;
        double yFraction = 0.0;

        if (sscanf(arg, "%lf %lf", &xFraction, &yFraction) != 2) {
            backdoor_write_result("ERROR: expected 'SCROLL <x 0.0-1.0> <y 0.0-1.0>'\n");
            return;
        }
        // set_[xy]_scroll_bar() take a position along the scrollbar track in logical pixels;
        // clamp_scroll_bar() inside them pins anything past the end, so scaling the fraction by
        // the render size is enough to reach either extreme.
        set_x_scroll_bar(xFraction * (get_render_width() / gGlobalGuiScale));
        set_y_scroll_bar(yFraction * (get_render_height() / gGlobalGuiScale));
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "ZOOM") == 0) {
        double zoom = 0.0;

        if (sscanf(arg, "%lf", &zoom) != 1) {
            backdoor_write_result("ERROR: expected 'ZOOM <0.25-2.0>'\n");
            return;
        }
        set_zoom_factor(zoom, (tCoord){0.0, 0.0});
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "SCREENSHOT") == 0) {
        if (arg[0] == '\0') {
            backdoor_write_result("ERROR: expected 'SCREENSHOT <path>'\n");
            return;
        }
        backdoor_screenshot(arg);
    } else {
        char msg[128];

        snprintf(msg, sizeof(msg), "ERROR: unknown command '%s'\n", cmd);
        backdoor_write_result(msg);
    }
}

static void backdoor_poll(void) {
    if (!backdoor_enabled()) {
        return;
    }
    const char * cmdPath   = backdoor_cmd_path();

    if (access(cmdPath, F_OK) != 0) {
        return;
    }
    FILE *       f         = fopen(cmdPath, "r");

    if (!f) {
        return;
    }
    char         line[512] = {0};

    if (!fgets(line, sizeof(line), f)) {
        line[0] = '\0';
    }
    fclose(f);
    remove(cmdPath);

    size_t       len       = strlen(line);

    while ((len > 0) && ((line[len - 1] == '\n') || (line[len - 1] == '\r'))) {
        line[--len] = '\0';
    }
    char         cmd[32]   = {0};
    char *       space     = strchr(line, ' ');

    if (space) {
        size_t cmdLen = (size_t)(space - line);

        if (cmdLen >= sizeof(cmd)) {
            cmdLen = sizeof(cmd) - 1;
        }
        memcpy(cmd, line, cmdLen);
        cmd[cmdLen] = '\0';
        backdoor_dispatch(cmd, space + 1);
    } else {
        strncpy(cmd, line, sizeof(cmd) - 1);
        backdoor_dispatch(cmd, "");
    }
}

void do_graphics_loop(void) {
    bool reDraw = false;

    while ((!synthlib_quit_requested()) && (!glfwWindowShouldClose((GLFWwindow *)synthlib_window()))) {
        check_action_flags();
        // Every registered popup's hover/dwell update, in one call. Polled every tick rather than only
        // on cursor movement, so a hover-dwell timer elapses while the mouse sits still — and the
        // host can no longer forget one, which is a bug that has shipped twice in this family of
        // apps. See synthlibPopups.h.
        synthlib_popups_tick();

        reDraw = synthlib_consume_redraw();

        if (reDraw == true) {
            render_frame();
        }
        // See the backdoor block's own header comment — a cheap no-op access()
        // check every iteration when idle, and completely skipped (returns
        // immediately) unless the G2_EDIT_BACKDOOR env var is set.
        backdoor_poll();

        // The Virtual Keyboard's Repeat button. No-op unless a repeat is actually running.
        virtual_keyboard_tick();

        // Belt and braces: a hidden pointer with no drag behind it never survives a frame, and nor
        // does a canvas gesture whose release went missing.
        recover_lost_cursor();

        // The selection is only valid for the slot, location and patch currently on screen.
        selection_validate();

        if ((gModuleDrag.active == true) || (gCableDrag.active == true) || (gContextMenu.active == true)) {
            tCoord at = {0};

            get_global_gui_scaled_mouse_coord(&at);
            cursor_pos(at);   // Artificially do cursor_pos call for drag scrolling when cursor not moving
            glfwWaitEventsTimeout(0.016);
        } else if (gDeviceOpInProgress > 0) {
            glfwWaitEventsTimeout(0.05); // tick while busy so the device-op safety timeout can fire even with no events
        } else if (virtual_keyboard_wants_ticks()) {
            glfwWaitEventsTimeout(0.02); // Repeat is running — glfwWaitEvents() would stall it until the next input event
        } else if (backdoor_enabled()) {
            glfwWaitEventsTimeout(0.1);  // poll cadence for the backdoor command file — only when enabled (owner's normal launch keeps full idle-sleep below)
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

    glfwDestroyWindow((GLFWwindow *)synthlib_window());
    synthlib_set_window(NULL);
    glfwTerminate();
}

#ifdef __cplusplus
}
#endif
