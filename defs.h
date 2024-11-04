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

#define INTERRUPT_MESSAGE_SIZE        (16)
#define EXTENDED_MESSAGE_SIZE         (8192)
#define SEND_MESSAGE_SIZE             (256)

#define RESPONSE_TYPE_EXTENDED        (0x1)
#define RESPONSE_TYPE_EMBEDDED        (0x2)
#define RESPONSE_TYPE_INIT            (0x80)
#define RESPONSE_TYPE_COMMAND         (0x01)
#define SUB_COMMAND_OK                (0x7f)
#define SUB_COMMAND_GET_PATCH_VERSION (0x36)
#define SUB_COMMAND_LED_DATA          (0x39)
#define SUB_COMMAND_VOLUME_INDICATOR  (0x3a)

#define COMMAND_REQ                   (0x20)  // High nibble, expects response
#define COMMAND_SYS                   (0x0c)  // Low nibble
#define COMMAND_SLOT                  (0x08)  // Low nibble

#define COMMAND_OFFSET                (2)     // Shouldn't need when write_uint16 is replaced by bit stream write

#define CRC_BYTES                     (2)


