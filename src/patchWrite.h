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

#ifndef PATCH_WRITE_H
#define PATCH_WRITE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool slot_has_engine_only_modules(uint32_t slot);
const char * patch_save_path(uint32_t slot, const char * path, char * out, size_t outSize);
const char * perf_save_path(const char * path, char * out, size_t outSize);
void perf_engine_only_header_apply(const uint8_t * header, size_t length);
void engine_only_header_apply(uint32_t slot, const uint8_t * header, size_t length);
int write_database_to_file(const char * filepath, uint32_t slot);  // EXIT_SUCCESS / EXIT_FAILURE
int write_perf_to_file(const char * filepath);                     // EXIT_SUCCESS / EXIT_FAILURE
uint8_t * write_perf_to_memory(size_t * sizeOut, bool engineOnly); // the .prf2 image; the caller frees it. notes §3

#ifdef __cplusplus
}
#endif

#endif // PATCH_WRITE_H
