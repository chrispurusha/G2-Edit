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

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int write_database_to_file(const char * filepath, uint32_t slot);  // EXIT_SUCCESS / EXIT_FAILURE
int write_perf_to_file(const char * filepath);                     // EXIT_SUCCESS / EXIT_FAILURE

#ifdef __cplusplus
}
#endif

#endif // PATCH_WRITE_H
