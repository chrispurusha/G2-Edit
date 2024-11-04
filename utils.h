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

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>


#define BIT_TO_BYTE(x) ((x) >> 3)  // Divide by 8
#define BYTE_TO_BIT(x) ((x) << 3)  // Multiply by 8

uint16_t crc_iterator(int32_t seed, int32_t val);
uint16_t calc_crc16(uint8_t * buff, int length);
void write_uint16(uint8_t * buff, uint16_t val);
uint32_t read_bit_stream(uint8_t * buff, uint32_t * bitPos, uint32_t numBits);
