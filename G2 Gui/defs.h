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

#ifndef __DEFS_H__
#define __DEFS_H__

#define ENABLE_DEBUG    // Comment out of not required

#define LOG_ERROR(fmt, ...)           fprintf(stderr, "E %s() " fmt, __func__, ## __VA_ARGS__)
#define LOG_WARNING(fmt, ...)         fprintf(stderr, "W %s() " fmt, __func__, ## __VA_ARGS__)
#define LOG_INFO(fmt, ...)            fprintf(stdout, "I %s() " fmt, __func__, ## __VA_ARGS__)
#ifdef ENABLE_DEBUG
#define LOG_DEBUG(fmt, ...)           fprintf(stdout, "D %s() " fmt, __func__, ## __VA_ARGS__)
#define LOG_DEBUG_DIRECT(fmt, ...)    fprintf(stdout, fmt, ## __VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)           ((void)0)
#define LOG_DEBUG_DIRECT(fmt, ...)    ((void)0)
#endif

#define TARGET_FRAME_BUFF_WIDTH              (2560)
#define TARGET_FRAME_BUFF_HEIGHT             (1440)
#define GLOBAL_GUI_SCALE                     (2)

#define NUM_VARIATIONS                       (10)  // 10 variations per patch, but only fist 8 presented on the GUI
#define NUM_GUI_VARIATIONS                   (8)
#define MAX_PARAMS_PER_MODULE                (38)
#define MAX_CONNECTORS_PER_MODULE            (10)
#define MAX_SLOTS                            (4)
#define NUM_MODES                            (16)

#define INTERRUPT_MESSAGE_SIZE               (16)
#define EXTENDED_MESSAGE_SIZE                (8192)
#define SEND_MESSAGE_SIZE                    (256)

#define RESPONSE_TYPE_EXTENDED               (0x1)
#define RESPONSE_TYPE_EMBEDDED               (0x2)
#define RESPONSE_TYPE_INIT                   (0x80)
#define RESPONSE_TYPE_COMMAND                (0x01)

#define SUB_COMMAND_GET_SYNTH_SETTINGS       (0x02)
#define SUB_RESPONSE_SYNTH_SETTINGS          (0x03)
#define SUB_COMMAND_SET_ASSIGNED_VOICES      (0x04)
#define SUB_RESPONSE_ASSIGNED_VOICES         (0x05)
#define SUB_COMMAND_SELECT_SLOT              (0x09)
#define SUB_COMMAND_RETRIEVE                 (0x0a)
#define SUB_COMMAND_STORE                    (0x0b)
#define SUB_COMMAND_CLEAR                    (0x0c)
#define SUB_RESPONSE_STORE                   (0x0d)
#define SUB_COMMAND_CLEAR_BANK               (0x0e)
#define SUB_RESPONSE_GLOBAL_PAGE             (0x1e)
#define SUB_RESPONSE_PATCH_DESCRIPTION       (0x21)
#define SUB_RESPONSE_PATCH_NAME              (0x27)
#define SUB_COMMAND_GET_PATCH_NAME           (0x28)
#define SUB_RESPONSE_PERFORMANCE_NAME        (0x29)
#define SUB_COMMAND_SET_MODULE_UPRATE        (0x2a)
#define SUB_COMMAND_SET_MODE                 (0x2b)
#define SUB_COMMAND_ADD_MODULE               (0x30)
#define SUB_COMMAND_SET_MODULE_COLOUR        (0x31)
#define SUB_COMMAND_DELETE_MODULE            (0x32)
#define SUB_COMMAND_MOVE_MODULE              (0x34)
#define SUB_COMMAND_GET_PATCH_VERSION        (0x35)
#define SUB_RESPONSE_PATCH_VERSION           (0x36)
#define SUB_RESPONSE_PATCH_VERSION_CHANGE    (0x38)
#define SUB_RESPONSE_LED_DATA                (0x39)
#define SUB_RESPONSE_VOLUME_INDICATOR        (0x3a)
#define SUB_COMMAND_GET_PATCH_SLOT           (0x3c)
#define SUB_RESPONSE_MASTER_CLOCK            (0x3f)
#define SUB_RESPONSE_PARAM_CHANGE            (0x40) // Seems to be used for response and set command
#define SUB_COMMAND_SET_PARAM                (0x40)
#define SUB_COMMAND_SET_PARAM_LABEL          (0x42)
#define SUB_COMMAND_SET_MORPH_RANGE          (0x43)
#define SUB_COMMAND_COPY_VARIATION           (0x44)
#define SUB_RESPONSE_MODULE_LIST             (0x4a)
#define SUB_RESPONSE_PARAM_LIST              (0x4d)
#define SUB_COMMAND_WRITE_CABLE              (0x50)
#define SUB_COMMAND_DELETE_CABLE             (0x51)
#define SUB_RESPONSE_CABLE_LIST              (0x52)
#define SUB_COMMAND_WRITE_CABLE_COLOUR       (0x54)
#define SUB_RESPONSE_MODULE_NAMES            (0x5a)
#define SUB_RESPONSE_PARAM_NAMES             (0x5b)
#define SUB_RESPONSE_MORPH_PARAMS            (0x65)
#define SUB_RESPONSE_RESOURCES_USED          (0x72)
#define SUB_COMMAND_START_STOP               (0x7d)
#define SUB_RESPONSE_ERROR                   (0x7e)
#define SUB_RESPONSE_OK                      (0x7f)
#define SUB_RESPONSE_MIDI_CC                 (0x80)

#define COMMAND_REQ                          (0x20) // High nibble, expects response
#define COMMAND_WRITE_NO_RESP                (0x30) // High nibble, expects response
#define COMMAND_SYS                          (0x0c) // Low nibble
#define COMMAND_SLOT                         (0x08) // Low nibble

#define COMMAND_OFFSET                       (2)    // Shouldn't need when write_uint16 is replaced by bit stream write

#define CRC_BYTES                            (2)

#define MODULE_NAME_SIZE                     (16)
#define PARAM_NAME_SIZE                      (16)

#define MAX_GLYPH_CHAR                       (127)

#define MAX_PARAM_RANGE                      (128)

#define MAX_COMPONENTS_PER_MODULE            (32)

#define BORDER_LINE_WIDTH                    (2.0)

#define RGB_BLACK                            {0.0, 0.0, 0.0}
#define RGB_WHITE                            {1.0, 1.0, 1.0}
#define RGB_GREEN                            {0.0, 0.8, 0.0}
#define RGB_BACKGROUND_GREY                  {0.8, 0.8, 0.8}
#define RGB_GREY_2                           {0.2, 0.2, 0.2}
#define RGB_GREY_3                           {0.3, 0.3, 0.3}
#define RGB_GREY_5                           {0.5, 0.5, 0.5}
#define RGB_GREY_7                           {0.7, 0.7, 0.7}
#define RGB_GREY_9                           {0.9, 0.9, 0.9}
#define RGB_GREEN_ON                         {0.3, 0.7, 0.3}
#define RGB_GREEN_3                          {0.0, 0.3, 0.0}
#define RGB_GREEN_7                          {0.0, 0.7, 0.0}
#define RGB_CONTEXT_MENU_GREEN               {0.2, 0.6, 0.2}
#define RGB_YELLOW_7                         {0.7, 0.7, 0.0}
#define RGB_RED_7                            {0.7, 0.0, 0.0}
#define RGB_ORANGE_2                         {0.8, 0.5, 0.2}
#define RGBA_BLACK_ON_TRANSPARENT            {0.0, 0.0, 0.0, 1.0}

// TODO: rationalise this lot
#define TOP_BAR_HEIGHT                       (80.0)
#define SCROLLBAR_WIDTH                      (15.0)
#define SCROLLBAR_LENGTH                     (100.0)
#define SCROLLBAR_MARGIN                     SCROLLBAR_WIDTH

#define MODULE_WIDTH                         (350.0)
#define MODULE_X_GAP                         (10.0)
#define MODULE_X_SPAN                        (MODULE_WIDTH + MODULE_X_GAP)
#define MODULE_TITLE_X_OFFSET                (3.0)
#define MODULE_HEIGHT                        (30.0) // 1 row
#define MODULE_MARGIN                        (5.0)
#define MODULE_Y_GAP                         (5.0)
#define MODULE_Y_SPAN                        (MODULE_HEIGHT + MODULE_Y_GAP)
#define MODULE_TITLE_Y_OFFSET                (20.0)
#define MODULE_AREA_X_MARGINS                ((MODULE_MARGIN * 2.0) + SCROLLBAR_WIDTH)
#define MODULE_AREA_Y_MARGINS                ((MODULE_MARGIN * 2.0) + TOP_BAR_HEIGHT + SCROLLBAR_WIDTH)
#define MODULE_AREA_X_WIDTH                  ((double)renderWidth - (MODULE_AREA_X_MARGINS))
#define MODULE_AREA_Y_HEIGHT                 ((double)renderHeight - (MODULE_AREA_Y_MARGINS))

#define COMP_MARGIN                          (5.0)

#define BLANK_SIZE                           (0.0) // Not used directly by calling function, or needs calculation in following calls
#define STANDARD_TEXT_HEIGHT                 (12.0)
#define STANDARD_BUTTON_TEXT_HEIGHT          (12.0)

#define CONNECTOR_SIZE                       (5)

#define MAX_ROWS                             (127)
#define MAX_COLUMNS                          (127)
#define MAX_ROWS_MODULE                      (12) // Operator type is 12 rows - largest we have

#define NULL_RECTANGLE                       {{0.0, 0.0}, {0.0, 0.0}}
#define ARRAY_SIZE(arr)    (sizeof(arr) / sizeof(arr[0]))

#endif // #define __DEFS_H__
