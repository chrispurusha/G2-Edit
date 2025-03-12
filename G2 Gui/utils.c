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

#include "utils.h"

uint16_t crc_iterator(int32_t seed, int32_t val) {
    int     i   = 0;
    int32_t crc = 0;
    int32_t k   = 0;

    k = (((seed >> 8) ^ val) & 255) << 8;

    for (i = 0; i < 8; i++) {
        if ((crc ^ k) & 0x8000) {
            crc = (crc << 1) ^ 0x1021;
        } else {
            crc = crc << 1;
        }
        k = k << 1;
    }

    return (uint16_t)((seed << 8) ^ crc) & 0xFFFF;
}

uint16_t calc_crc16(uint8_t * buff, int length) {
    uint16_t crc  = 0;
    int      i    = 0;
    uint8_t  byte = 0;

    if (buff == NULL) {
        return 0;
    }

    for (i = 0; i < length; i++) {
        byte = buff[i];
        crc  = crc_iterator((int32_t)crc, (int32_t)byte);
    }

    return crc;
}

void write_uint16(uint8_t * buff, uint16_t val) {
    if (buff == NULL) {
        return;
    }
    buff[0] = (val >> 8) & 0xff;
    buff[1] = val & 0xff;
}

uint32_t read_bit_stream(uint8_t * buff, uint32_t * bitPos, uint32_t numBits) {
    int      i   = 0;
    uint32_t val = 0;

    if ((buff == NULL) || (bitPos == NULL) || (numBits > 32) || (numBits == 0)) {
        return 0;
    }

    for (i = 0; i < numBits; i++) {
        val |= ((buff[((*bitPos) >> 3)] >> ((7 - *bitPos) & 0x07)) & 0x01) << ((numBits - 1) - i);
        (*bitPos)++;
    }

    return val;
}

#ifdef __cplusplus
}
#endif
