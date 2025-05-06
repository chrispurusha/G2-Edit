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

#define VARIATIONS                        (10) // 10 variations per patch, but only fist 8 presented on the GUI
#define MAX_PARAMS_PER_MODULE             (38)
#define MAX_CONNECTORS_PER_MODULE         (10)
#define MAX_SLOTS                         (4)
#define NUM_MODES                         (16)

#define INTERRUPT_MESSAGE_SIZE            (16)
#define EXTENDED_MESSAGE_SIZE             (8192)
#define SEND_MESSAGE_SIZE                 (256)

#define RESPONSE_TYPE_EXTENDED            (0x1)
#define RESPONSE_TYPE_EMBEDDED            (0x2)
#define RESPONSE_TYPE_INIT                (0x80)
#define RESPONSE_TYPE_COMMAND             (0x01)
#define SUB_COMMAND_OK                    (0x7f)
#define SUB_COMMAND_GET_PATCH_VERSION     (0x36)
#define SUB_COMMAND_LED_DATA              (0x39)
#define SUB_COMMAND_VOLUME_INDICATOR      (0x3a)
#define SUB_COMMAND_WRITE_MODULE          (0x30)
#define SUB_COMMAND_DELETE_MODULE         (0x32)
#define SUB_COMMAND_MOVE_MODULE           (0x34)
#define SUB_COMMAND_SET_PARAM             (0x40)
#define SUB_COMMAND_SET_MODE              (0x2b)
#define SUB_COMMAND_SET_MODULE_UPRATE     (0x2a)
#define SUB_COMMAND_WRITE_CABLE           (0x50)
#define SUB_COMMAND_DELETE_CABLE          (0x51)
#define SUB_COMMAND_WRITE_CABLE_COLOUR    (0x54)
#define SUB_COMMAND_SET_MODE              (0x2b) // Works similar to set param

#define COMMAND_REQ                       (0x20) // High nibble, expects response
#define COMMAND_WRITE_NO_RESP             (0x30) // High nibble, expects response
#define COMMAND_SYS                       (0x0c) // Low nibble
#define COMMAND_SLOT                      (0x08) // Low nibble

#define COMMAND_OFFSET                    (2)    // Shouldn't need when write_uint16 is replaced by bit stream write

#define CRC_BYTES                         (2)

#define MODULE_NAME_SIZE                  (16)

#define MAX_GLYPH_CHAR                    (128)

#define MAX_PARAM_RANGE                   (128)

#define MAX_COMPONENTS_PER_MODULE         (32)

#define BORDER_LINE_WIDTH                 (2.0)

#define RGB_BLACK                         {0.0, 0.0, 0.0}
#define RGB_WHITE                         {1.0, 1.0, 1.0}
#define RGB_BACKGROUND_GREY               {0.8, 0.8, 0.8}
#define RGBA_BLACK_ON_TRANSPARENT         {0.0, 0.0, 0.0, 1.0}

// Todo - rationalise this lot
#define TOP_BAR_HEIGHT                    (80.0)
#define SCROLLBAR_WIDTH                   (15.0)
#define SCROLLBAR_LENGTH                  (100.0)
#define SCROLLBAR_MARGIN                  SCROLLBAR_WIDTH

#define MODULE_WIDTH                      (350.0)
#define MODULE_X_GAP                      (10.0)
#define MODULE_X_SPAN                     (MODULE_WIDTH + MODULE_X_GAP)
#define MODULE_TITLE_X_OFFSET             (3.0)
#define MODULE_HEIGHT                     (30.0) // 1 row
#define MODULE_MARGIN                     (5.0)
#define MODULE_Y_GAP                      (5.0)
#define MODULE_Y_SPAN                     (MODULE_HEIGHT + MODULE_Y_GAP)
#define MODULE_TITLE_Y_OFFSET             (20.0)
#define MODULE_AREA_X_MARGINS             ((MODULE_MARGIN * 2.0) + SCROLLBAR_WIDTH)
#define MODULE_AREA_Y_MARGINS             ((MODULE_MARGIN * 2.0) + TOP_BAR_HEIGHT + SCROLLBAR_WIDTH)
#define MODULE_AREA_X_WIDTH               ((double)renderWidth - (MODULE_AREA_X_MARGINS))
#define MODULE_AREA_Y_HEIGHT              ((double)renderHeight - (MODULE_AREA_Y_MARGINS))

#define COMP_MARGIN                       (5.0)


#define BYPASS_BUTTON_WIDTH               (15.0)
#define BYPASS_BUTTON_HEIGHT              (15.0)
#define GC_BUTTON_WIDTH                   (25.0)
#define GC_BUTTON_HEIGHT                  (15.0)
#define FILTER_FREQ_RADIUS                (12.0)
#define FILTER_RESONANCE_RADIUS           (12.0)
#define BLANK_SIZE                        (0.0)   // Not used directly by calling function, or needs calculation in following calls
#define STANDARD_TEXT_HEIGHT              (12.0)
#define STANDARD_BUTTON_TEXT_HEIGHT       (12.0)

#define CONNECTOR_RADIUS                  (8.0)
#define CONNECTOR_DIAMETER                (CONNECTOR_RADIUS * 2.0)

#define MAX_ROWS                          (127)
#define MAX_COLUMNS                       (127)
#define MAX_ROWS_MODULE                   (12)    // Operator type is 12 rows - largest we have

#define NULL_RECTANGLE                    {{0.0, 0.0}, {0.0, 0.0}}
#define ARRAY_SIZE(arr)    (sizeof(arr) / sizeof(arr[0]))

#endif // #define __DEFS_H__
