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
// Notes: Docs/code-notes/patchWrite.c.md - "// notes §k" refers there.

// notes §1

#include "sysIncludes.h"
#include "defs.h"
#include "types.h"
#include "utils.h"
#include "protocol.h"
#include "dataBase.h"
#include "globalVars.h"
#include "patchWrite.h"

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

// The whole .prf2 image - text header, binary, CRC - in a buffer the caller frees. NULL on failure.
uint8_t * write_perf_to_memory(size_t * sizeOut) {
    static const char header[] = "Version=Nord Modular G2 File Format 1\r\n"
                                 "Type=Performance\r\n"
                                 "Version=23\r\n"
                                 "Info=BUILD 320\r\n";
    size_t            headLen  = sizeof(header);    // with the terminating NUL, which the format keeps
    uint32_t          bitPos   = 0;
    uint32_t          calcCrc  = 0;
    // calloc, not malloc and memset: the work buffer is large, and zeroed pages cost nothing until used
    uint8_t *         buff     = (uint8_t *)calloc(1, PERF_FILE_SIZE);

    if (sizeOut != NULL) {
        *sizeOut = 0;
    }

    if (buff == NULL) {
        LOG_ERROR("Memory allocation failed\n");
        return NULL;
    }
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

    bitPos  = BYTE_TO_BIT(BIT_TO_BYTE_ROUND_UP(bitPos));
    calcCrc = calc_crc16(buff, BIT_TO_BYTE_ROUND_UP(bitPos));
    write_bit_stream(buff, &bitPos, 16, calcCrc);

    size_t    binLen = BIT_TO_BYTE_ROUND_UP(bitPos);

    if (binLen > ((PERF_FILE_SIZE * 3) / 4)) {
        LOG_ERROR("Write file size > 3/4 of %d, might need to increase PERF_FILE_SIZE\n", PERF_FILE_SIZE);
    }
    uint8_t * image  = (uint8_t *)malloc(headLen + binLen);

    if (image != NULL) {
        memcpy(image, header, headLen);
        memcpy(image + headLen, buff, binLen);

        if (sizeOut != NULL) {
            *sizeOut = headLen + binLen;
        }
    }
    free(buff);
    return image;
}

int write_perf_to_file(const char * filepath) {
    size_t    size    = 0;
    uint8_t * image   = write_perf_to_memory(&size);
    FILE *    file    = NULL;

    if (image == NULL) {
        return EXIT_FAILURE;
    }
    file = fopen(filepath, "wb");

    if (!file) {
        LOG_ERROR("Error opening file\n");
        free(image);
        return EXIT_FAILURE;
    }
    size_t    written = fwrite(image, 1, size, file);

    if (written != size) {
        LOG_ERROR("Written %zu of %zu\n", written, size);
    }
    free(image);
    fclose(file);
    return (written == size) ? EXIT_SUCCESS : EXIT_FAILURE;
}
