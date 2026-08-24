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

#ifdef __cplusplus
extern "C" {
#endif

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
// GLFW here is for its KEY CONSTANTS only — no GLFW function is called.
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include "helpPanel.h"
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibGlobals.h"
#include "utilsGraphics.h"

tHelpPanel            gHelpPanel    = {0};

// One row of the reference. A NULL keys field makes the row a section heading; a NULL text field with
// NULL keys is a blank spacer.
typedef struct {
    const char * keys;
    const char * text;
} tHelpRow;

// EVERY ROW HERE IS A CLAIM ABOUT THE CODE. Where each comes from, so the next person changing a
// binding knows which file to check: the canvas and shortcut rows are mouseHandle.c (key_event,
// mouse_button, scroll_event) and canvasDrag.c (the gesture table); the note-entry rows are
// virtualKeyboard.c (note_offset_for_key and handle_note_entry_key); the panel rows are
// floatingPanel.c; the variation rows are mouseTopbar.c and protocol.c's fan-out.
static const tHelpRow kLeftColumn[] = {
    {NULL,                 "CANVAS"                                     },
    {"Click",              "Select module"                              },
    {"Shift / Cmd click",  "Add to or remove from the selection"        },
    {"Drag empty canvas",  "Rubber-band select"                         },
    {"Shift drag",         "Rubber-band adds to the selection"          },
    {"Right-click module", "Module menu"                                },
    {"Right-click canvas", "Create module"                              },
    {"Delete / Backspace", "Delete the selection"                       },
    {"Cmd A",              "Select all"                                 },
    {"Cmd C / X / V",      "Copy / Cut / Paste"                         },
    {"Cmd Z",              "Undo"                                       },
    {"Shift Cmd Z",        "Redo"                                       },
    {NULL,                 NULL                                         },

    {NULL,                 "PARAMETERS"                                 },
    {"Drag a dial",        "Change the value"                           },
    {"Shift drag",         "Finer adjustment"                           },
    {"Alt drag",           "Morph range, for the focused morph group"   },
    {"+ / -",              "Step the parameter under the pointer"       },
    {"Alt wheel",          "Step the parameter under the pointer"       },
    {"Right-click a dial", "Parameter menu: knob, MIDI CC, morph"       },
    {"Up / Down",          "Step the focused parameter"                 },
    {"Left / Right",       "Focus the previous / next parameter"        },
    {"Shift arrows",       "Focus another module"                       },
    {"L",                  "MIDI Learn for the focused parameter"       },
    {NULL,                 "Click a parameter to focus it - or step one"},
    {NULL,                 "with + / -. Focus shows as corner marks."   },
    {NULL,                 NULL                                         },

    {NULL,                 "VIEW"                                       },
    {"Wheel",              "Scroll the pane under the pointer"          },
    {"Cmd wheel",          "Zoom"                                       },
    {"Cmd + / -",          "Zoom in / out"                              },
    {NULL,                 NULL                                         },
};

static const tHelpRow kRightColumn[] = {
    {NULL,                 "PLAYING FROM THE KEYBOARD"                          },
    {"A S D F G H J K",    "White keys, C upwards"                              },
    {"W E   T Y U   O P",  "Black keys"                                         },
    {NULL,                 "L is MIDI Learn, not a note."                       },
    {"Z / X",              "Octave down / up"                                   },
    {"Shift + note",       "Hold the note; press it again without Shift to stop"},
    {NULL,                 "Works with the Virtual Keyboard panel closed."      },
    {NULL,                 NULL                                                 },

    {NULL,                 "VARIATIONS"                                         },
    {"Click 1-8",          "Select the variation"                               },
    {"Shift click 1-8",    "Mark it: edits also go to every marked variation"   },
    {NULL,                 "Marked is orange; marked and selected is split."    },
    {"Right-click 1-8",    "Copy this variation to another"                     },
    {NULL,                 NULL                                                 },

    {NULL,                 "PANELS"                                             },
    {"Drag the title bar", "Move a panel"                                       },
    {"Ctrl drag",          "Move a panel from anywhere on its face"             },
    {"Click a panel",      "Bring it to the front"                              },
    {"Escape",             "Close the panel"                                    },
    {NULL,                 NULL                                                 },

    {NULL,                 "FILE"                                               },
    {"Cmd O / S / N",      "Open / Save / New patch"                            },
    {"Cmd ,",              "Synth settings"                                     },
    {"Cmd 2",              "Patch Mutator"                                      },
    {NULL,                 NULL                                                 },
};

#define HELP_ROWS(a)    (sizeof(a) / sizeof((a)[0]))
#define HELP_KEYS_W     (150.0)
#define HELP_COL_GAP    (26.0)

void open_help_panel(void) {
    floating_panel_raise(&gHelpPanel.panel);   // opens in front — see floatingPanel.c on why this is not left to placement
    gHelpPanel.active       = true;
    gHelpPanel.closePressed = false;
}

void close_help_panel(void) {
    gHelpPanel.active = false;
}

static double column_width(const tHelpRow * rows, uint32_t count, double textH) {
    double widest = 0.0;

    for (uint32_t i = 0; i < count; i++) {
        if (rows[i].text != NULL) {
            double w = get_text_width((char *)rows[i].text, textH, eNoCache);

            // A heading or a note spans the whole column; a description starts past the key column.
            if (rows[i].keys != NULL) {
                w += HELP_KEYS_W;
            }

            if (w > widest) {
                widest = w;
            }
        }
    }

    return widest;
}

static void draw_column(double x, double y, double rowH, double textH, const tHelpRow * rows, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        if (rows[i].text != NULL) {
            double textX = (rows[i].keys != NULL) ? (x + HELP_KEYS_W) : x;

            if (rows[i].keys != NULL) {
                render_text(mainArea, (tRectangle){{x, y}, {BLANK_SIZE, textH}}, (char *)rows[i].keys);
            }
            render_text(mainArea, (tRectangle){{textX, y}, {BLANK_SIZE, textH}}, (char *)rows[i].text);
        }
        y += rowH;
    }
}

void render_help_panel(void) {
    if (!gHelpPanel.active) {
        return;
    }
    double     margin     = 14.0;
    double     titleH     = 24.0;
    double     textH      = STANDARD_TEXT_HEIGHT;
    double     rowH       = textH + 5.0;

    uint32_t   leftCount  = (uint32_t)HELP_ROWS(kLeftColumn);
    uint32_t   rightCount = (uint32_t)HELP_ROWS(kRightColumn);

    double     leftW      = column_width(kLeftColumn, leftCount, textH);
    double     rightW     = column_width(kRightColumn, rightCount, textH);

    // Sized to the taller column so neither is clipped — the two are close but not equal, and which
    // one is taller changes whenever a row is added.
    uint32_t   rows       = (leftCount > rightCount) ? leftCount : rightCount;
    double     boxW       = (margin * 2.0) + leftW + HELP_COL_GAP + rightW;
    double     boxH       = titleH + margin + ((double)rows * rowH) + margin;

    tRectangle box        = floating_panel_place(&gHelpPanel.panel, boxW, boxH);

    gHelpPanel.panel.titleBarRect = draw_panel_chrome(mainArea, box, titleH, "Keyboard and Mouse");
    gHelpPanel.close              = draw_panel_close_button(mainArea, box, gHelpPanel.closePressed);
    gHelpPanel.panel.closeRect    = gHelpPanel.close;

    double     y          = box.coord.y + titleH + margin;

    draw_column(box.coord.x + margin, y, rowH, textH, kLeftColumn, leftCount);
    draw_column(box.coord.x + margin + leftW + HELP_COL_GAP, y, rowH, textH, kRightColumn, rightCount);
}

bool handle_help_panel_mouse(tCoord coord, tMouseButton mouseButton) {
    if (!gHelpPanel.active) {
        return false;
    }

    // Claims only its own clicks — see the same guard in virtualKeyboard.c for why a floating panel
    // must not swallow everything the way the modal versions did.
    if (mouseButton == mouseButtonLeftDown) {
        if (floating_panel_press(&gHelpPanel.panel, coord)) {
            return true;
        }

        if (!floating_panel_contains(&gHelpPanel.panel, coord)) {
            return false;
        }

        if (within_rectangle(coord, gHelpPanel.close)) {
            gHelpPanel.closePressed = true;
        }
        synthlib_request_redraw();
        return true;
    }

    if (mouseButton == mouseButtonLeftUp) {
        bool wasDragging = gHelpPanel.panel.dragging;

        floating_panel_release(&gHelpPanel.panel);

        if (wasDragging) {
            return true;
        }

        if (gHelpPanel.closePressed) {
            gHelpPanel.closePressed = false;

            if (within_rectangle(coord, gHelpPanel.close)) {
                close_help_panel();
            }
            synthlib_request_redraw();
            return true;
        }
        return floating_panel_contains(&gHelpPanel.panel, coord);
    }
    return false;
}

bool handle_help_panel_key(int key, int mods, int action) {
    (void)mods;

    if (!gHelpPanel.active || (action != GLFW_PRESS) || (key != GLFW_KEY_ESCAPE)) {
        return false;
    }
    close_help_panel();
    return true;
}

#ifdef __cplusplus
}
#endif
