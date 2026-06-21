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
 * Static menu item arrays for synth settings dropdowns.
 * Included only by menus.c, after action_setting_u8 / action_setting_i8 are defined.
 */

static tMenuItem gMidiChanItems[]      = {
    {"Off", RGB_GREY_3, action_setting_u8, 0x10, NULL},
    {"1",   RGB_GREY_3, action_setting_u8,    0, NULL},
    {"2",   RGB_GREY_3, action_setting_u8,    1, NULL},
    {"3",   RGB_GREY_3, action_setting_u8,    2, NULL},
    {"4",   RGB_GREY_3, action_setting_u8,    3, NULL},
    {"5",   RGB_GREY_3, action_setting_u8,    4, NULL},
    {"6",   RGB_GREY_3, action_setting_u8,    5, NULL},
    {"7",   RGB_GREY_3, action_setting_u8,    6, NULL},
    {"8",   RGB_GREY_3, action_setting_u8,    7, NULL},
    {"9",   RGB_GREY_3, action_setting_u8,    8, NULL},
    {"10",  RGB_GREY_3, action_setting_u8,    9, NULL},
    {"11",  RGB_GREY_3, action_setting_u8,   10, NULL},
    {"12",  RGB_GREY_3, action_setting_u8,   11, NULL},
    {"13",  RGB_GREY_3, action_setting_u8,   12, NULL},
    {"14",  RGB_GREY_3, action_setting_u8,   13, NULL},
    {"15",  RGB_GREY_3, action_setting_u8,   14, NULL},
    {"16",  RGB_GREY_3, action_setting_u8,   15, NULL},
    {NULL,  {0,                         0,   0}, NULL, 0, NULL}, };

static tMenuItem gSysexIdItems[]       = {
    {"1",   RGB_GREY_3, action_setting_u8,  0, NULL},
    {"2",   RGB_GREY_3, action_setting_u8,  1, NULL},
    {"3",   RGB_GREY_3, action_setting_u8,  2, NULL},
    {"4",   RGB_GREY_3, action_setting_u8,  3, NULL},
    {"5",   RGB_GREY_3, action_setting_u8,  4, NULL},
    {"6",   RGB_GREY_3, action_setting_u8,  5, NULL},
    {"7",   RGB_GREY_3, action_setting_u8,  6, NULL},
    {"8",   RGB_GREY_3, action_setting_u8,  7, NULL},
    {"9",   RGB_GREY_3, action_setting_u8,  8, NULL},
    {"10",  RGB_GREY_3, action_setting_u8,  9, NULL},
    {"11",  RGB_GREY_3, action_setting_u8, 10, NULL},
    {"12",  RGB_GREY_3, action_setting_u8, 11, NULL},
    {"13",  RGB_GREY_3, action_setting_u8, 12, NULL},
    {"14",  RGB_GREY_3, action_setting_u8, 13, NULL},
    {"15",  RGB_GREY_3, action_setting_u8, 14, NULL},
    {"16",  RGB_GREY_3, action_setting_u8, 15, NULL},
    {"All", RGB_GREY_3, action_setting_u8, 16, NULL},
    {NULL,  {0,                         0, 0}, NULL, 0, NULL}, };

static tMenuItem gOctaveShiftItems[]   = {
    {"-2", RGB_GREY_3, action_setting_i8, (uint32_t)(int32_t)-2, NULL},
    {"-1", RGB_GREY_3, action_setting_i8, (uint32_t)(int32_t)-1, NULL},
    {"0",  RGB_GREY_3, action_setting_i8,                     0, NULL},
    {"+1", RGB_GREY_3, action_setting_i8,                     1, NULL},
    {"+2", RGB_GREY_3, action_setting_i8,                     2, NULL},
    {NULL, {0,                         0,                    0}, NULL, 0, NULL}, };

static tMenuItem gPatchSortItems[]     = {
    {"Prog#",    RGB_GREY_3, action_setting_u8, 0, NULL},
    {"Alpha",    RGB_GREY_3, action_setting_u8, 1, NULL},
    {"Category", RGB_GREY_3, action_setting_u8, 2, NULL},
    {NULL,       {0,                         0,0}, NULL, 0, NULL}, };

static tMenuItem gPerfSortItems[]      = {
    {"Prog#",    RGB_GREY_3, action_setting_u8, 0, NULL},
    {"Category", RGB_GREY_3, action_setting_u8, 2, NULL},
    {NULL,       {0,                         0,0}, NULL, 0, NULL}, };

static tMenuItem gOnOffItems[]         = {
    {"Off", RGB_GREY_3, action_setting_u8, 0, NULL},
    {"On",  RGB_GREY_3, action_setting_u8, 1, NULL},
    {NULL,  {0,                         0,0}, NULL, 0, NULL}, };

static tMenuItem gActiveOffItems[]     = {
    {"Off",    RGB_GREY_3, action_setting_u8, 0, NULL},
    {"Active", RGB_GREY_3, action_setting_u8, 1, NULL},
    {NULL,     {0,                         0,0}, NULL, 0, NULL}, };

static tMenuItem gPedalPolarityItems[] = {
    {"Open",   RGB_GREY_3, action_setting_u8, 0, NULL},
    {"Closed", RGB_GREY_3, action_setting_u8, 1, NULL},
    {NULL,     {0,                         0,0}, NULL, 0, NULL}, };

static tMenuItem gPatchTypeItems[]     = {
    {"No Cat",    RGB_GREY_3, action_set_patch_type, patchTypeNoCat,     NULL},
    {"Acoustic",  RGB_GREY_3, action_set_patch_type, patchTypeAcoustic,  NULL},
    {"Sequencer", RGB_GREY_3, action_set_patch_type, patchTypeSequencer, NULL},
    {"Bass",      RGB_GREY_3, action_set_patch_type, patchTypeBass,      NULL},
    {"Classic",   RGB_GREY_3, action_set_patch_type, patchTypeClassic,   NULL},
    {"Drum",      RGB_GREY_3, action_set_patch_type, patchTypeDrum,      NULL},
    {"Fantasy",   RGB_GREY_3, action_set_patch_type, patchTypeFantasy,   NULL},
    {"Fx",        RGB_GREY_3, action_set_patch_type, patchTypeFx,        NULL},
    {"Lead",      RGB_GREY_3, action_set_patch_type, patchTypeLead,      NULL},
    {"Organ",     RGB_GREY_3, action_set_patch_type, patchTypeOrgan,     NULL},
    {"Pad",       RGB_GREY_3, action_set_patch_type, patchTypePad,       NULL},
    {"Piano",     RGB_GREY_3, action_set_patch_type, patchTypePiano,     NULL},
    {"Synth",     RGB_GREY_3, action_set_patch_type, patchTypeSynth,     NULL},
    {"Audio In",  RGB_GREY_3, action_set_patch_type, patchTypeAudioIn,   NULL},
    {"User 1",    RGB_GREY_3, action_set_patch_type, patchTypeUser1,     NULL},
    {"User 2",    RGB_GREY_3, action_set_patch_type, patchTypeUser2,     NULL},
    {NULL,        RGB_BLACK,  NULL,                                   0, NULL}, };

static tMenuItem gMonoPolyItems[]      = {
    {"Poly",   RGB_GREY_3, action_set_mono_poly, monoPolyPoly,   NULL},
    {"Mono",   RGB_GREY_3, action_set_mono_poly, monoPolyMono,   NULL},
    {"Legato", RGB_GREY_3, action_set_mono_poly, monoPolyLegato, NULL},
    {NULL,     RGB_BLACK,  NULL,                              0, NULL}, };
