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
// Notes: Docs/code-notes/floatingPanels.c.md - "// notes §k" refers there.

// THE COORDINATOR OVER THE FLOATING PANELS, moved out of graphics.c on 2026-09-16 so that the
// plug-in gets it too: it lived in the application's render loop, so in the plug-in every Settings
// and Tools menu entry opened a panel that was never drawn and never saw a click. The four
// callbacks each host registers are in floatingPanels.h.

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "geometry.h"
#include "globalVars.h"
#include "synthlibGlobals.h"
#include "utilsGraphics.h"
#include "mouseHandle.h"    // get_global_gui_scaled_mouse_coord() - the app's and the plug-in's
#include "floatingPanel.h"
#include "settingsPanels.h"
#include "mousePanels.h"
#include "mutatorUI.h"
#include "virtualKeyboard.h"
#include "patchAdjuster.h"
#include "helpPanel.h"
#include "paramPages.h"
#include "paramOverview.h"
#include "midiCcList.h"
#include "floatingPanels.h"

// notes §1
static tFloatingPanelEntry gFloatingPanels[] = {
    {&gVirtualKeyboard.panel,   render_virtual_keyboard_panel, handle_virtual_keyboard_mouse, handle_virtual_keyboard_key, &gVirtualKeyboard.active  },
    {&gPatchAdjuster.panel,     render_patch_adjuster_panel,   handle_patch_adjuster_mouse,   handle_patch_adjuster_key,   &gPatchAdjuster.active    },
    {&gHelpPanel.panel,         render_help_panel,             handle_help_panel_mouse,       handle_help_panel_key,       &gHelpPanel.active        },
    {&gMutator.panel,           render_mutator_panel,          handle_mutator_mouse,          handle_mutator_key,          &gMutator.active          },
    {&gPatchSettingsEdit.panel, render_patch_settings_panel,   handle_patch_settings_mouse,   handle_patch_settings_key,   &gPatchSettingsEdit.active},
    {&gPerfSettingsEdit.panel,  render_perf_settings_panel,    handle_perf_settings_mouse,    handle_perf_settings_key,    &gPerfSettingsEdit.active },
    {&gPatchParamsEdit.panel,   render_patch_params_panel,     handle_patch_params_mouse,     handle_patch_params_key,     &gPatchParamsEdit.active  },

    // notes §2
    {&gPatchNotesEdit.panel,    render_patch_notes_edit,       handle_patch_notes_mouse,      NULL,                        &gPatchNotesEdit.active   },
    {&gParamPages.panel,        render_param_pages_panel,      handle_param_pages_mouse,      handle_param_pages_key,      &gParamPages.active       },
    {&gParamOverview.panel,     render_param_overview_panel,   handle_param_overview_mouse,   handle_param_overview_key,   &gParamOverview.active    },
    {&gMidiCcList.panel,        render_midi_cc_list_panel,     handle_midi_cc_list_mouse,     handle_midi_cc_list_key,     &gMidiCcList.active       }
};

#define FLOATING_PANEL_COUNT    ((uint32_t)(sizeof(gFloatingPanels) / sizeof(gFloatingPanels[0])))

// WHICH panel, front to back — the same walk the clicks take, so the panel this names is the panel
// that would receive a press.
static const tFloatingPanel * floating_panel_at(tCoord coord) {
    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = FLOATING_PANEL_COUNT; i > 0; i--) {
        if (  floating_panel_entry_visible(&gFloatingPanels[i - 1])
           && floating_panel_contains(gFloatingPanels[i - 1].panel, coord)) {
            return gFloatingPanels[i - 1].panel;
        }
    }

    return NULL;
}

bool floating_panels_under(tCoord coord) {
    return floating_panel_at(coord) != NULL;
}

// notes §3
static void panel_press_takes_the_keyboard(tCoord coord) {
    const tFloatingPanel * hit = floating_panel_at(coord);

    if (hit == NULL) {
        return;
    }
    stop_patch_name_editing();
    stop_module_name_editing();
    stop_param_name_editing();
    stop_perf_name_editing();

    if (hit != &gPatchSettingsEdit.panel) {
        stop_synth_name_editing();
    }
}

// notes §4
static void raise_newly_shown_panels(void) {
    static const tFloatingPanel * wasVisible[FLOATING_PANEL_COUNT] = {NULL};
    static uint32_t               wasVisibleCount                  = 0;
    const tFloatingPanel *        nowVisible[FLOATING_PANEL_COUNT] = {NULL};
    uint32_t                      nowVisibleCount                  = 0;

    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (!floating_panel_entry_visible(&gFloatingPanels[i])) {
            continue;
        }
        const tFloatingPanel * panel = gFloatingPanels[i].panel;
        bool                   seen  = false;

        for (uint32_t j = 0; j < wasVisibleCount; j++) {
            if (wasVisible[j] == panel) {
                seen = true;
                break;
            }
        }

        if (!seen) {
            floating_panel_raise(gFloatingPanels[i].panel);
        }
        nowVisible[nowVisibleCount++] = panel;
    }

    for (uint32_t i = 0; i < nowVisibleCount; i++) {
        wasVisible[i] = nowVisible[i];
    }

    wasVisibleCount = nowVisibleCount;
}

void floating_panels_render(void) {
    raise_newly_shown_panels();

    // notes §5
    floating_panel_set_bounds((tRectangle){{
                                               0.0, 0.0
                                           }, {
                                               (get_render_width() / gGlobalGuiScale) - SCROLLBAR_WIDTH,
                                               (get_render_height() / gGlobalGuiScale) - SCROLLBAR_WIDTH
                                           }
                              });

    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (gFloatingPanels[i].render != NULL) {
            gFloatingPanels[i].render();     // back to front, so the most recently clicked ends up on top
        }
    }
}

// Reversed against the draw walk: sorted back-to-front for drawing, so front-to-back is the
// hit-test order. Fixed call order was wrong the moment two of them could overlap — whichever was
// tested first swallowed the press, even when it was the one underneath.
bool floating_panels_mouse(tCoord coord, tMouseButton mouseButton) {
    if (mouseButton == mouseButtonLeftDown) {
        panel_press_takes_the_keyboard(coord);
    }
    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = FLOATING_PANEL_COUNT; i > 0; i--) {
        if (  (gFloatingPanels[i - 1].mouse != NULL)
           && gFloatingPanels[i - 1].mouse(coord, mouseButton)) {
            return true;
        }
    }

    return false;
}

// Keys are ordered for the same reason clicks are: Escape has to close the panel you are LOOKING at.
// Fixed call order closed whichever handler came first — with the Help panel in front and the
// Virtual Keyboard behind it, Escape shut the keyboard.
bool floating_panels_key(int key, int mods, int action) {
    floating_panel_sort(gFloatingPanels, FLOATING_PANEL_COUNT);

    for (uint32_t i = FLOATING_PANEL_COUNT; i > 0; i--) {
        // notes §6
        if (  (gFloatingPanels[i - 1].key != NULL)
           && gFloatingPanels[i - 1].key(key, mods, action)) {
            return true;
        }
    }

    return false;
}

// notes §7
bool floating_panel_is_frontmost(const tFloatingPanel * panel) {
    const tFloatingPanel * front = NULL;

    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (!floating_panel_entry_visible(&gFloatingPanels[i])) {
            continue;   // a closed panel keeps its order, so it must not win this
        }

        // notes §8
        if ((front == NULL) || !floating_panel_in_front_of(front, gFloatingPanels[i].panel)) {
            front = gFloatingPanels[i].panel;
        }
    }

    return (front != NULL) && (front == panel);
}

// notes §9

// notes §10
bool floating_panels_drag(tCoord coord) {
    for (uint32_t i = 0; i < FLOATING_PANEL_COUNT; i++) {
        if (floating_panel_drag(gFloatingPanels[i].panel, coord)) {
            return true;
        }
    }

    return false;
}

// notes §11
bool floating_panels_scroll(double yDelta) {
    tCoord coord = {0};

    (void)yDelta;
    get_global_gui_scaled_mouse_coord(&coord);
    return floating_panels_under(coord);
}

#ifdef __cplusplus
}
#endif
