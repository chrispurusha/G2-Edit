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

/*
 * Reference credit on some of the excellent G2 comms protocol work by
 * Bruno Verhue in his Delphi editor application:
 *
 * https://www.bverhue.nl/g2dev/
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "defs.h"
#include "types.h"
#include <libusb-1.0/libusb.h>
#include "utils.h"
#include "msgQueue.h"
#include "usbComms.h"
#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "globalVars.h"

void parse_patch_descr(uint8_t * buff, uint32_t * subOffset) {
    gPatchDescr[gSlot].unknown1        = read_bit_stream(buff, subOffset, 32);
    gPatchDescr[gSlot].unknown2        = read_bit_stream(buff, subOffset, 29);
    gPatchDescr[gSlot].voiceCount      = read_bit_stream(buff, subOffset, 5);
    gPatchDescr[gSlot].barPosition     = read_bit_stream(buff, subOffset, 14);
    gPatchDescr[gSlot].unknown3        = read_bit_stream(buff, subOffset, 3);
    gPatchDescr[gSlot].redVisible      = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].blueVisible     = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].yellowVisible   = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].orangeVisible   = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].greenVisible    = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].purpleVisible   = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].whiteVisible    = read_bit_stream(buff, subOffset, 1);
    gPatchDescr[gSlot].monoPoly        = read_bit_stream(buff, subOffset, 2);
    gPatchDescr[gSlot].activeVariation = read_bit_stream(buff, subOffset, 8);
    gPatchDescr[gSlot].category        = read_bit_stream(buff, subOffset, 8);
    gPatchDescr[gSlot].unknown4        = read_bit_stream(buff, subOffset, 12);

    LOG_DEBUG("  Voice Count %u\n", gPatchDescr[gSlot].voiceCount);
    LOG_DEBUG("  Bar Position %u\n", gPatchDescr[gSlot].barPosition);
    LOG_DEBUG("  Red %u\n", gPatchDescr[gSlot].redVisible);
    LOG_DEBUG("  Blue %u\n", gPatchDescr[gSlot].blueVisible);
    LOG_DEBUG("  Yellow %u\n", gPatchDescr[gSlot].yellowVisible);
    LOG_DEBUG("  Orange %u\n", gPatchDescr[gSlot].orangeVisible);
    LOG_DEBUG("  Green %u\n", gPatchDescr[gSlot].greenVisible);
    LOG_DEBUG("  Purple %u\n", gPatchDescr[gSlot].purpleVisible);
    LOG_DEBUG("  White %u\n", gPatchDescr[gSlot].whiteVisible);
    LOG_DEBUG("  Mono Poly %u\n", gPatchDescr[gSlot].monoPoly);
    LOG_DEBUG("  Active Variation %u\n", gPatchDescr[gSlot].activeVariation);
    LOG_DEBUG("  Category %u\n", gPatchDescr[gSlot].category);
    
    // TODO - Might want to reconsider how we do this, since there's multiple cases of this setting of button colour
    for (uint32_t i = 0; i < NUM_GUI_VARIATIONS; i++) {
        gMainButtonArray[(uint32_t)variation1ButtonId + i].backgroundColour = (tRgb)RGB_BACKGROUND_GREY;
    }

    gMainButtonArray[gPatchDescr[gSlot].activeVariation+(uint32_t)variation1ButtonId].backgroundColour = (tRgb)RGB_GREEN_ON;
}
    
void write_patch_descr(uint8_t * buff, uint32_t * bitPos) {
    write_bit_stream(buff, bitPos, 8, SUB_RESPONSE_PATCH_DESCRIPTION);
    write_bit_stream(buff, bitPos, 16, 15); // Length of following in bytes
    write_bit_stream(buff, bitPos, 32, gPatchDescr[gSlot].unknown1);
    write_bit_stream(buff, bitPos, 29, gPatchDescr[gSlot].unknown2);
    write_bit_stream(buff, bitPos, 5, gPatchDescr[gSlot].voiceCount);
    write_bit_stream(buff, bitPos, 14, gPatchDescr[gSlot].barPosition);
    write_bit_stream(buff, bitPos, 3, gPatchDescr[gSlot].unknown3);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].redVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].blueVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].yellowVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].orangeVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].greenVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].purpleVisible);
    write_bit_stream(buff, bitPos, 1, gPatchDescr[gSlot].whiteVisible);
    write_bit_stream(buff, bitPos, 2, gPatchDescr[gSlot].monoPoly);
    write_bit_stream(buff, bitPos, 8, gPatchDescr[gSlot].activeVariation);
    write_bit_stream(buff, bitPos, 8, gPatchDescr[gSlot].category);
    write_bit_stream(buff, bitPos, 12, gPatchDescr[gSlot].unknown4);
    
    // TODO: Possibly pad, using something similar to BIT_TO_BYTE_ROUND_UP(bitPos). Maybe pass in buffer size, for over-write checks
}

#ifdef __cplusplus
}
#endif

