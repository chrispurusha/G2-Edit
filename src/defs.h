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

#define PATCH_FILE_SIZE                      (10 * 1024 * 1024)
#define TARGET_FRAME_BUFF_WIDTH              (2560)
#define TARGET_FRAME_BUFF_HEIGHT             (1440)
//#define GLOBAL_GUI_SCALE                     (2)  // Should be related to window size

#define WINDOW_TITLE                         "G2 Editor"

#define NUM_VARIATIONS_USB                   (10) // 10 variations per patch, but only fist 8 presented on the GUI
#define NUM_VARIATIONS                       (9)  // One less variation for file access or without the USB extra = 9
#define VARIATION_INIT                       (8)
#define NUM_MORPHS                           (8)  // Not sure if we can go higher with this, so remember to check
#define MAX_PARAMS_PER_MODULE                (38)
#define MAX_CONNECTORS_PER_MODULE            (10)
#define MAX_SLOTS                            (4)
#define MAX_NUM_MODES                        (16)
#define MAX_NUM_PARAMETERS                   (128)
#define MAX_NUM_LABELS                       (32)
#define MAX_NUM_CONNECTORS                   (13)
#define MAX_NUM_KNOBS                        (120)                        // G2 always has exactly 120 knob slots
#define MAX_NUM_CONTROLLERS                  (128)
#define NUM_PARAM_PAGES                      (5)
#define NUM_BANKS_PER_PAGE                   (3)
#define NUM_KNOBS_PER_BANK                   (8)

#define INTERRUPT_MESSAGE_SIZE               (16)
#define EXTENDED_MESSAGE_SIZE                (65536)
#define SEND_MESSAGE_SIZE                    (65536)

#define RESPONSE_TYPE_EXTENDED               (0x1)
#define RESPONSE_TYPE_EMBEDDED               (0x2)
#define RESPONSE_TYPE_INIT                   (0x80)
#define RESPONSE_TYPE_COMMAND                (0x01)

#define SUB_RESPONSE_NULL                    (0x00)
#define SUB_COMMAND_GET_SYNTH_SETTINGS       (0x02)
#define SUB_COMMAND_SET_SYNTH_SETTINGS       (0x03)
#define SUB_RESPONSE_SYNTH_SETTINGS          (0x03)
#define SUB_COMMAND_SET_ASSIGNED_VOICES      (0x04)
#define SUB_COMMAND_GET_ASSIGNED_VOICES      (0x04)
#define SUB_RESPONSE_SET_ASSIGNED_VOICES     (0x04)
#define SUB_RESPONSE_ASSIGNED_VOICES         (0x05)
#define SUB_COMMAND_SELECT_SLOT              (0x09)
#define SUB_RESPONSE_SELECT_SLOT             (0x09)
#define SUB_COMMAND_RETRIEVE                 (0x0a)
#define SUB_COMMAND_STORE                    (0x0b)
#define SUB_COMMAND_CLEAR                    (0x0c)
#define SUB_RESPONSE_STORE                   (0x0d)
#define SUB_COMMAND_CLEAR_BANK               (0x0e)
#define SUB_COMMAND_PERFORMANCE_SETTINGS     (0x10)
#define SUB_COMMAND_SET_KEY_SPLIT            (0x11)
#define SUB_RESPONSE_STORE_PATCH             (0x13)
#define SUB_COMMAND_ASSIGN_GLOBAL_KNOB       (0x1c)
#define SUB_COMMAND_DEASSIGN_GLOBAL_KNOB     (0x1d)
#define SUB_RESPONSE_GLOBAL_PAGE             (0x1e)
#define SUB_RESPONSE_PERF_PATCH_VERSIONS     (0x1f)
#define SUB_RESPONSE_PATCH_DESCRIPTION       (0x21) // Think this is also used for set patch description also response to 0x3c 'get patch slot', also used when hide all 00 0f 00 00 00 00 00 00 00 00 42 58 5f d0 00 f0 00
#define SUB_COMMAND_ASSIGN_MIDICC            (0x22)
#define SUB_COMMAND_DEASSIGN_MIDICC          (0x23)
#define SUB_COMMAND_ASSIGN_KNOB              (0x25)
#define SUB_COMMAND_DEASSIGN_KNOB            (0x26)
#define SUB_COMMAND_SET_PATCH_NAME           (0x27)
#define SUB_RESPONSE_GET_PATCH_NAME          (0x27) // Note - same value as command to set patch name
#define SUB_COMMAND_GET_PATCH_NAME           (0x28)
#define SUB_RESPONSE_PERFORMANCE_SETTINGS    (0x29)
#define SUB_COMMAND_SET_MODULE_UPRATE        (0x2a)
#define SUB_COMMAND_SET_MODE                 (0x2b)
#define SUB_RESPONSE_SEL_PARAM_PAGE          (0x2d)
#define SUB_COMMAND_GET_SELECTED_PARAM       (0x2e)
#define SUB_RESPONSE_SELECT_PARAM            (0x2F)
#define SUB_COMMAND_ADD_MODULE               (0x30)
#define SUB_COMMAND_SET_MODULE_COLOUR        (0x31)
#define SUB_COMMAND_DELETE_MODULE            (0x32)
#define SUB_COMMAND_SET_MODULE_LABEL         (0x33)
#define SUB_COMMAND_MOVE_MODULE              (0x34)
#define SUB_COMMAND_GET_PATCH_VERSION        (0x35)
#define SUB_RESPONSE_PATCH_VERSION           (0x36)
#define SUB_COMMAND_SET_PATCH                (0x37) // Upload patch to slot, so send all the data clearing existing patch!?
#define SUB_RESPONSE_PATCH_VERSION_CHANGE    (0x38)
#define SUB_RESPONSE_LED_DATA                (0x39)
#define SUB_RESPONSE_VOLUME_INDICATOR        (0x3a)
#define SUB_COMMAND_QUERY_MASTER_CLOCK       (0x3b)
#define SUB_COMMAND_GET_PATCH_SLOT           (0x3c)
#define SUB_COMMAND_MIDI_DUMP                (0x3d)
#define SUB_COMMAND_SET_PARAM_MODE           (0x3e)
#define SUB_RESPONSE_MASTER_CLOCK            (0x3f)
#define SUB_COMMAND_SET_MASTER_CLOCK         (0x3f) // Note - same value as SUB_RESPONSE_MASTER_CLOCK
#define SUB_RESPONSE_PARAM_CHANGE            (0x40) // Seems to be used for response and set command
#define SUB_COMMAND_SET_PARAM                (0x40)
#define SUB_COMMAND_SET_PERF_MODE            (0x41) // Followed by 3e 01 00 to enable perf,  35 04 to disable?
#define SUB_COMMAND_SET_PARAM_LABEL          (0x42)
#define SUB_COMMAND_SET_MORPH_RANGE          (0x43)
#define SUB_COMMAND_COPY_VARIATION           (0x44)
#define SUB_RESPONSE_MODULE_LIST             (0x4a)
#define SUB_COMMAND_QUERY_PARAMS             (0x4c)
#define SUB_RESPONSE_PARAM_LIST              (0x4d)
#define SUB_COMMAND_QUERY_PARAM_NAMES        (0x4f)
#define SUB_COMMAND_WRITE_CABLE              (0x50)
#define SUB_COMMAND_DELETE_CABLE             (0x51)
#define SUB_RESPONSE_CABLE_LIST              (0x52)
#define SUB_COMMAND_WRITE_CABLE_COLOUR       (0x54)
#define SUB_COMMAND_CTRL_SNAPSHOT            (0x55)
#define SUB_COMMAND_PLAY_NOTE                (0x56)
#define SUB_COMMAND_GET_GLOBAL_PAGE          (0x59) // Possibly read global page, since response indicates that coming back
#define SUB_RESPONSE_MODULE_NAMES            (0x5a)
#define SUB_RESPONSE_PARAM_NAMES             (0x5b)
#define SUB_RESPONSE_EXT_MASTER_CLOCK        (0x5d)
#define SUB_COMMAND_QUERY_GLOBAL_KNOBS       (0x5e) // Q_GLOBAL_KNOBS in Delphi — query sent to G2
#define SUB_RESPONSE_GLOBAL_KNOBS            (0x5f) // C_KNOBS_GLOBAL in Delphi — response from G2
#define SUB_RESPONSE_CONTROLLERS             (0x60)
#define SUB_RESPONSE_KNOBS                   (0x62)
#define SUB_RESPONSE_MORPH_PARAMS            (0x65)
#define SUB_COMMAND_CURRENT_NOTE             (0x68)
#define SUB_RESPONSE_CURRENT_NOTE_2          (0x69)
#define SUB_COMMAND_SELECT_VARIATION         (0x6a)
#define SUB_RESPONSE_SELECT_VARIATION        (0x6a)
#define SUB_COMMAND_QUERY_PATCH_TEXT         (0x6e)
#define SUB_RESPONSE_PATCH_NOTES             (0x6f)
#define SUB_COMMAND_KNOB_SNAPSHOT            (0x70)  // Unknown 6, receives C_KNOBS ?
#define SUB_COMMAND_QUERY_RESOURCES          (0x71)
#define SUB_RESPONSE_RESOURCES_USED          (0x72)
#define SUB_COMMAND_START_STOP               (0x7d)
#define SUB_RESPONSE_ERROR                   (0x7e)
#define SUB_RESPONSE_OK                      (0x7f)
#define SUB_RESPONSE_MIDI_CC                 (0x80)
#define SUB_COMMAND_GET_MIDI_CC              (0x81) // A.k.a. Unknown 1!?

#define SUB_RESPONSE_PERF_SETTINGS           (0x11)
#define SUB_RESPONSE_CLEAR_BANK              (0x12)
#define SUB_RESPONSE_LIST_NAMES              (0x13)
#define SUB_COMMAND_LIST_NAMES               (0x14)
#define SUB_RESPONSE_CLEAR                   (0x15)
#define SUB_RESPONSE_ADD_NAMES               (0x16)
#define SUB_COMMAND_PATCH_BANK_UPLOAD        (0x17)
#define SUB_RESPONSE_PATCH_BANK_UPLOAD       (0x18)
#define SUB_COMMAND_PATCH_BANK_DATA          (0x19)

#define COMMAND_REQ                          (0x20) // High nibble, expects response
#define COMMAND_WRITE_NO_RESP                (0x30) // High nibble, expects response
#define COMMAND_SYS                          (0x0c) // Low nibble
#define COMMAND_SLOT                         (0x08) // Low nibble

#define COMMAND_OFFSET                       (2)    // Shouldn't need when write_uint16 is replaced by bit stream write

#define CRC_BYTES                            (2)

// Patch settings module indices (locationMorph, location 2)
// These are module indices within the patch settings location - TODO - should probably be enumerations in types.h
#define PATCH_MORPH                    (0x01)
#define PATCH_VOLUME                   (0x02)
#define PATCH_GLIDE                    (0x03)
#define PATCH_BEND                     (0x04)
#define PATCH_VIBRATO                  (0x05)
#define PATCH_ARPEGGIATOR              (0x06)
#define PATCH_SUSTAIN                  (0x07)
#define PATCH_MASTERCLOCK              (0x08)
#define PATCH_VOICES                   (0x09)

// PATCH_VOLUME param indices
#define VOLUME_LEVEL                   (0x00)
#define VOLUME_MUTE                    (0x01)

// PATCH_GLIDE param indices
#define GLIDE_TYPE                     (0x00)
#define GLIDE_SPEED                    (0x01)

// PATCH_BEND param indices
#define BEND_ON_OFF                    (0x00)
#define BEND_RANGE                     (0x01)

// PATCH_VIBRATO param indices
#define VIBRATO_MOD                    (0x00)
#define VIBRATO_DEPTH                  (0x01)
#define VIBRATO_RATE                   (0x02)

// PATCH_ARPEGGIATOR param indices
#define ARP_ON_OFF                     (0x00)
#define ARP_SPEED                      (0x01)
#define ARP_DIRECTION                  (0x02)
#define ARP_OCTAVES                    (0x03)

// PATCH_SUSTAIN param indices
#define OCTAVE_SHIFT                   (0x00)
#define SUSTAIN_PEDAL                  (0x01)

#define CLAVIA_NAME_SIZE               (16)          // Don't forget to add null terminator where necessary
//#define MODULE_NAME_SIZE               (16)
//#define PARAM_NAME_SIZE                (16)
#define PROTOCOL_PARAM_NAME_SIZE       (7)
#define PATCH_NOTES_SIZE               (1024)       // OG editor gets to about 1084 before crashing

#define MAX_GLYPH_CHAR                 (127)

#define MAX_PARAM_RANGE                (128)

#define MAX_COMPONENTS_PER_MODULE      (32)

#define BORDER_LINE_WIDTH              (2.0)

#define RGB_BLACK                      {0.0, 0.0, 0.0}
#define RGB_WHITE                      {1.0, 1.0, 1.0}
#define RGB_GREEN                      {0.0, 0.8, 0.0}
#define RGB_BACKGROUND_GREY            {0.8, 0.8, 0.8}
#define RGB_GREY_2                     {0.2, 0.2, 0.2}
#define RGB_GREY_3                     {0.3, 0.3, 0.3}
#define RGB_GREY_5                     {0.5, 0.5, 0.5}
#define RGB_GREY_7                     {0.7, 0.7, 0.7}
#define RGB_GREY_9                     {0.9, 0.9, 0.9}
#define RGB_GREEN_ON                   {0.3, 0.7, 0.3}
#define RGB_GREEN_3                    {0.0, 0.3, 0.0}
#define RGB_GREEN_7                    {0.0, 0.7, 0.0}
#define RGB_CONTEXT_MENU_GREEN         {0.2, 0.6, 0.2}
#define RGB_YELLOW_7                   {0.7, 0.7, 0.0}
#define RGB_RED_7                      {0.7, 0.0, 0.0}
#define RGB_RED_5                      {0.7, 0.2, 0.2}
#define RGB_ORANGE_1                   {0.8, 0.3, 0.1}
#define RGB_ORANGE_2                   {0.8, 0.5, 0.2}
#define RGBA_BLACK_ON_TRANSPARENT      {0.0, 0.0, 0.0, 1.0}

#define MODULE_STANDARD_GREY           {0.7, 0.7, 0.7}
#define MODULE_RED_4                   {0.8, 0.6, 0.6}
#define MODULE_GREEN_4                 {0.6, 0.8, 0.6}
#define MODULE_BLUE_4                  {0.6, 0.6, 0.8}
#define MODULE_YELLOW_4                {0.8, 0.8, 0.6}
#define MODULE_BLUE_1                  {0.3, 0.3, 0.8}
#define MODULE_RED_1                   {0.8, 0.3, 0.3}
#define MODULE_CYAN_2                  {0.4, 0.8, 0.8}
#define MODULE_GREEN_2                 {0.4, 0.8, 0.4}
#define MODULE_YELLOW_1                {0.8, 0.8, 0.3}
#define MODULE_GREEN_1                 {0.3, 0.8, 0.3}
#define MODULE_YELLOW_2                {0.8, 0.8, 0.4}
#define MODULE_BLUE_3                  {0.5, 0.5, 0.8}
#define MODULE_RED_2                   {0.8, 0.4, 0.4}
#define MODULE_RED_3                   {0.8, 0.5, 0.5}
#define MODULE_YELLOW_3                {0.8, 0.8, 0.5}
#define MODULE_GREEN_3                 {0.5, 0.8, 0.5}
#define MODULE_CYAN_1                  {0.3, 0.8, 0.8}
#define MODULE_CYAN_3                  {0.5, 0.8, 0.8}
#define MODULE_CYAN_4                  {0.6, 0.8, 0.8}
#define MODULE_BLUE_2                  {0.4, 0.4, 0.8}
#define MODULE_PURPLE_1                {0.8, 0.3, 0.8}
#define MODULE_PURPLE_2                {0.8, 0.4, 0.8}
#define MODULE_PURPLE_3                {0.8, 0.5, 0.8}
#define MODULE_PURPLE_4                {0.8, 0.6, 0.8}

// TODO: rationalise this lot
#define TOP_BAR_HEIGHT                 (80.0)
#define SCROLLBAR_WIDTH                (15.0)
#define SCROLLBAR_LENGTH               (100.0)
#define SCROLLBAR_MARGIN               SCROLLBAR_WIDTH

#define MODULE_WIDTH                   (350.0)
#define MODULE_X_GAP                   (10.0)
#define MODULE_X_SPAN                  (MODULE_WIDTH + MODULE_X_GAP)
#define MODULE_TITLE_X_OFFSET          (3.0)
#define MODULE_HEIGHT                  (38.0)       // 1 row
#define MODULE_MARGIN                  (5.0)
#define MODULE_Y_GAP                   (5.0)
#define MODULE_Y_SPAN                  (MODULE_HEIGHT + MODULE_Y_GAP)
#define MODULE_TITLE_Y_OFFSET          (20.0)
#define MODULE_AREA_X_MARGINS          ((MODULE_MARGIN * 2.0) + SCROLLBAR_WIDTH)
#define MODULE_AREA_Y_MARGINS          ((MODULE_MARGIN * 2.0) + TOP_BAR_HEIGHT + SCROLLBAR_WIDTH)
#define MODULE_AREA_X_WIDTH            ((double)renderWidth - (MODULE_AREA_X_MARGINS))
#define MODULE_AREA_Y_HEIGHT           ((double)renderHeight - (MODULE_AREA_Y_MARGINS))

#define NO_ZOOM                        (1.0)
#define ZOOM_DELTA                     (0.1)

#define COMP_MARGIN                    (5.0)

#define BLANK_SIZE                     (0.0)       // Not used directly by calling function, or needs calculation in following calls
#define STANDARD_TEXT_HEIGHT           (12.0)
#define STANDARD_BUTTON_TEXT_HEIGHT    (12.0)

#define CONNECTOR_SIZE                 (5)

#define MAX_ROWS                       (127)
#define MAX_COLUMNS                    (127)
#define MAX_ROWS_MODULE                (12)       // Operator type is 12 rows - largest we have

#define LONGEST_PATCH_NAME             "XXXXXXXXXXXXXXXX"
#define LONGEST_MODULE_NAME            "XXXXXXXXXXXXXXXX"

#define NULL_RECTANGLE                 {{0.0, 0.0}, {0.0, 0.0}}
#define ARRAY_SIZE(arr)    (sizeof(arr) / sizeof(arr[0]))

#endif // #define __DEFS_H__
