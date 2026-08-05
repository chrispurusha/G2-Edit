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

#ifndef __MENUS_H__
#define __MENUS_H__

#include "sysIncludes.h"
#include "contextMenu.h" // Generic mac-style nested menu mechanism + tMenuItem/tMenuFrame/tContextMenu — see SynthLib

#ifdef __cplusplus
extern "C" {
#endif

// Module creation utilities
int32_t create_module_at(tModuleType type, uint32_t column, uint32_t row, bool syncToDevice);
void convert_mouse_coord_to_module_column_row(uint32_t * column, uint32_t * row, tCoord coord);
void shift_modules_down(tModuleKey key);
int32_t find_unique_module_id(uint32_t location);
void open_module_area_context_menu(tCoord coord);

// Parameter / knob menus
int32_t find_knob_for_param(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex);
int32_t find_global_knob_for_param(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex);
int32_t find_controller_for_param(uint32_t slot, uint32_t location, uint32_t moduleIndex, uint32_t paramIndex);

// Bulk MIDI CC tools, the original editor's "Assign MIDI" / "Clear MIDI". Each is one undoable
// operation over the whole Slot. The Slot is explicit rather than gSlot because the Parameter
// Overview panel offers these on whichever Slot IT is showing, which need not be the selected one.
void midi_cc_assign_all_knobs(uint32_t slot);
void midi_cc_clear_all(uint32_t slot);

// The original's "Assign MIDI to Selection" / "Deassign MIDI from Selection". NOT the same thing as
// the pair above: these cover every parameter of the selected modules, assigned to a panel knob or
// not, where those two only ever touch the 120 panel knob assignments. Always the selected Slot,
// since that is the only Slot a selection can be in.
void midi_cc_assign_selection(void);
void midi_cc_deassign_selection(void);
void open_param_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t paramIndex);
void open_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t paramIndex, uint32_t paramRef);
void open_mode_toggle_menu(tCoord coord, tModuleKey moduleKey, uint32_t modeIndex, uint32_t modeRef);

// Module / cable / morph menus
void open_connector_context_menu(tCoord coord, tModuleKey moduleKey, uint32_t connectorIndex);
void open_module_context_menu(tCoord coord, tModuleKey moduleKey);
void open_morph_label_context_menu(tCoord coord, uint32_t morphIndex);

// Patch settings menus
void open_patch_type_context_menu(tCoord coord);
void open_mono_poly_context_menu(tCoord coord);
void open_voice_count_context_menu(tCoord coord);
void open_variation_copy_menu(tCoord coord, uint32_t sourceVariation);

// Synth settings dropdowns
void send_synth_settings_msg(void);
void send_perf_settings_msg(void);
void send_master_clock_run(uint32_t running);
void open_midi_chan_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_sysex_id_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_tune_semi_dropdown(tCoord coord, _Atomic int8_t * target);
void open_tune_cent_dropdown(tCoord coord, _Atomic int8_t * target);
void open_octave_shift_dropdown(tCoord coord, _Atomic int8_t * target);
void open_pedal_gain_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_patch_sort_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_perf_sort_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_on_off_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_active_off_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_pedal_polarity_dropdown(tCoord coord, _Atomic uint8_t * target);

// Perf settings dropdowns
void open_perf_on_off_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_stop_run_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_master_clock_dropdown(tCoord coord, _Atomic uint8_t * target);
void open_midi_note_dropdown(tCoord coord, _Atomic uint8_t * target);

// Patch settings dropdowns
void open_patch_on_off_dropdown(tCoord coord, uint32_t moduleIndex, uint32_t paramIndex);
void toggle_patch_on_off(uint32_t moduleIndex, uint32_t paramIndex);
void open_arp_rate_dropdown(tCoord coord);
void open_arp_direction_dropdown(tCoord coord);
void open_arp_octave_dropdown(tCoord coord);
void open_vibrato_source_dropdown(tCoord coord);
void open_vibrato_amount_dropdown(tCoord coord);
void open_glide_mode_dropdown(tCoord coord);
void open_glide_time_dropdown(tCoord coord);
void open_bend_range_dropdown(tCoord coord);
void open_patch_octave_shift_dropdown(tCoord coord);

#ifdef __cplusplus
}
#endif


// Assign one parameter to one MIDI CC#, stealing it from whatever held it. Shared by the right-click
// assign menu and by MIDI Learn.
void assign_midi_cc_to_param(uint32_t slot, tModuleKey moduleKey, uint32_t paramIndex, uint32_t targetCC);

// MIDI Learn (the L key): assign the last-clicked parameter to the last-received MIDI CC#. False,
// with a reason logged, when there is no focused parameter or no CC has arrived.
bool midi_learn_focused_param(void);

// Every parameter click goes through this. If L was pressed with nothing to act on, the click takes
// the CC; otherwise it does nothing, so the two can be done in either order.
void midi_learn_param_clicked(tModuleKey moduleKey, uint32_t paramIndex);
bool midi_learn_armed(void);

// The CC MIDI Learn would use, and where it came from ("synth" or "MIDI in"), or -1 if none has been
// seen. The synth's own report wins: it is the thing with a MIDI IN socket.
int32_t midi_learn_last_cc(const char ** source);


#endif // __MENUS_H__
