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
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include <math.h>

#include "virtualKeyboard.h"
#include "defs.h"
#include "synthlibDefs.h"
#include "synthlibGlobals.h"
#include "globalVars.h"
#include "graphics.h"
#include "msgQueue.h"
#include "utilsGraphics.h"

tVirtualKeyboard  gVirtualKeyboard = {0};

// Which semitones of an octave are black keys. Index by note % 12.
static const bool kIsBlack[12]     = {false, true, false, true, false, false, true, false, true, false, true, false};

// How far a black key sits from the left edge of the white key it follows, as a fraction of a white
// key's width. A real keyboard doesn't centre them on the gap — C# and D# sit slightly left and
// right of centre respectively, and the same within the F-A# group — but centring is what every
// software keyboard does and it keeps the hit rects honest against what's drawn.
#define VKB_BLACK_W_FRAC    (0.62)
#define VKB_BLACK_H_FRAC    (0.62)

#define VKB_WHITE_W         (22.0)
#define VKB_WHITE_H         (86.0)
#define VKB_NOTE_MAX        (127)

// Repeat's rate. A GUESS: the manual says only "play repeatedly" and gives no rate, and the
// original offers no control over it either. 250ms is a musically plausible eighth-note-ish pulse
// and is slow enough that the note is clearly re-struck rather than buzzing. If the real editor
// turns out to lock this to the master clock, this is the constant to replace.
#define VKB_REPEAT_MS    (250.0)

// ─── Lifecycle ───────────────────────────────────────────────────────────────

void open_virtual_keyboard_panel(void) {
    gVirtualKeyboard.active       = true;
    gVirtualKeyboard.closePressed = false;
    gVirtualKeyboard.noteOn       = -1;
    gVirtualKeyboard.lastNote     = -1;
    // Drone and Repeat deliberately do NOT survive a close: both leave the synth making a noise,
    // and reopening the panel to find it immediately droning would be a nasty surprise.
    gVirtualKeyboard.drone        = false;
    gVirtualKeyboard.repeat       = false;

    if (gVirtualKeyboard.velocity == 0) {
        gVirtualKeyboard.velocity  = 100;
        gVirtualKeyboard.firstNote = 48;   // C3, so the default span is C3..C6 — middle of the range
    }
}

// ─── Note sending ────────────────────────────────────────────────────────────

static void send_note(uint32_t note, bool on) {
    tMessageContent msg = {0};

    msg.cmd                   = eMsgCmdPlayNote;
    msg.playNoteData.note     = note;
    msg.playNoteData.velocity = gVirtualKeyboard.velocity;
    msg.playNoteData.on       = on;
    msg_send(&gToUsbThread, &msg);
}

// The single point at which the sounding note changes. Every path goes through here — closing the
// panel, scrolling the range, pressing another key — so a note can never be left sounding with no
// key showing it. Passing -1 means silence.
static void set_sounding_note(int32_t note) {
    if (gVirtualKeyboard.noteOn == note) {
        return;
    }

    if (gVirtualKeyboard.noteOn >= 0) {
        send_note((uint32_t)gVirtualKeyboard.noteOn, false);
    }

    if (note >= 0) {
        send_note((uint32_t)note, true);
    }
    gVirtualKeyboard.noteOn = note;
}

void close_virtual_keyboard_panel(void) {
    // Order matters: clear the toggles first, or a droning note would survive the silence below in
    // spirit — nothing re-sends it, but reopening would inherit an engaged Drone.
    gVirtualKeyboard.drone  = false;
    gVirtualKeyboard.repeat = false;
    set_sounding_note(-1);
    gVirtualKeyboard.active = false;
}

// Repeat re-strikes the last played note on a timer. Note-off then note-on rather than a bare
// note-on, so the patch's envelopes actually retrigger.
void virtual_keyboard_tick(void) {
    if (!gVirtualKeyboard.active || !gVirtualKeyboard.repeat || (gVirtualKeyboard.lastNote < 0)) {
        return;
    }
    double now = glfwGetTime() * 1000.0;

    if (now < gVirtualKeyboard.nextRepeatAt) {
        return;
    }
    gVirtualKeyboard.nextRepeatAt = now + VKB_REPEAT_MS;

    // Straight through send_note() rather than set_sounding_note(), which would see the same note
    // twice running and do nothing. noteOn is already this note and stays that way, so the key
    // keeps its highlight between strikes instead of flickering.
    if (gVirtualKeyboard.noteOn >= 0) {
        send_note((uint32_t)gVirtualKeyboard.noteOn, false);
    }
    send_note((uint32_t)gVirtualKeyboard.lastNote, true);
    gVirtualKeyboard.noteOn       = gVirtualKeyboard.lastNote;
    synthlib_request_redraw();
}

bool virtual_keyboard_wants_ticks(void) {
    return gVirtualKeyboard.active && gVirtualKeyboard.repeat && (gVirtualKeyboard.lastNote >= 0);
}

// Scrolls the visible span, clamped so it never runs off either end of the MIDI range. Releases any
// sounding note first: the key it belongs to may be about to leave the panel.
static void scroll_by(int32_t semitones) {
    int32_t first = (int32_t)gVirtualKeyboard.firstNote + semitones;
    int32_t last  = VKB_NOTE_MAX - (VKB_KEYS_VISIBLE - 1);

    set_sounding_note(-1);

    if (first < 0) {
        first = 0;
    }

    if (first > last) {
        first = last;
    }
    gVirtualKeyboard.firstNote = (uint32_t)first;
}

// ─── Rendering ───────────────────────────────────────────────────────────────

void render_virtual_keyboard_panel(void) {
    if (!gVirtualKeyboard.active) {
        return;
    }
    double   renderW = get_render_width() / gGlobalGuiScale;
    double   renderH = get_render_height() / gGlobalGuiScale;
    double   margin  = 10.0;
    double   titleH  = 24.0;
    double   btnH    = STANDARD_BUTTON_TEXT_HEIGHT;
    double   textH   = STANDARD_TEXT_HEIGHT;

    // Count the whites in the visible span first — the panel is exactly as wide as they need.
    uint32_t whites  = 0;

    for (uint32_t i = 0; i < VKB_KEYS_VISIBLE; i++) {
        uint32_t note = gVirtualKeyboard.firstNote + i;

        if ((note <= VKB_NOTE_MAX) && !kIsBlack[note % 12]) {
            whites++;
        }
    }

    double   whiteW  = VKB_WHITE_W;
    double   boxW    = (margin * 2.0) + (whiteW * whites);
    double   boxH    = titleH + margin + btnH + margin + VKB_WHITE_H + margin + textH + margin;

    if (boxW > (renderW - (margin * 2.0))) {
        boxW   = renderW - (margin * 2.0);
        whiteW = (boxW - (margin * 2.0)) / (double)whites;
    }
    double   boxX    = (renderW - boxW) / 2.0;
    double   boxY    = (renderH - boxH) / 2.0;
    double   y       = boxY + titleH + margin;

    draw_dialog_background_overlay();
    draw_panel_chrome(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, titleH, "Virtual Keyboard");
    gVirtualKeyboard.close = draw_panel_close_button(mainArea, (tRectangle){{boxX, boxY}, {boxW, boxH}}, gVirtualKeyboard.closePressed);

    // ── The button bar ────────────────────────────────────────────────────
    // Four scroll buttons on the left as in the original — double arrows an octave, singles a note
    // — then Drone and Repeat, which are toggles and so carry the lit/unlit colour every other
    // toggle in the app uses.
    {
        double x       = boxX + margin;
        double bw      = get_text_width((char *)"<<", btnH, eCache) + 14.0;
        double droneW  = get_text_width((char *)"Drone", btnH, eCache) + 14.0;
        double repeatW = get_text_width((char *)"Repeat", btnH, eCache) + 14.0;

        gVirtualKeyboard.octaveDown   = draw_button(mainArea, (tRectangle){{x, y}, {bw, btnH}}, "<<", (tRgb)RGB_BACKGROUND_GREY);
        x                            += bw + 4.0;
        gVirtualKeyboard.noteDown     = draw_button(mainArea, (tRectangle){{x, y}, {bw, btnH}}, "<", (tRgb)RGB_BACKGROUND_GREY);
        x                            += bw + 4.0;
        gVirtualKeyboard.noteUp       = draw_button(mainArea, (tRectangle){{x, y}, {bw, btnH}}, ">", (tRgb)RGB_BACKGROUND_GREY);
        x                            += bw + 4.0;
        gVirtualKeyboard.octaveUp     = draw_button(mainArea, (tRectangle){{x, y}, {bw, btnH}}, ">>", (tRgb)RGB_BACKGROUND_GREY);

        // Right-aligned, so the scroll cluster and the sound toggles don't read as one group of six.
        double bx      = boxX + boxW - margin - repeatW;

        gVirtualKeyboard.repeatButton = draw_button(mainArea, (tRectangle){{bx, y}, {repeatW, btnH}},
                                                    "Repeat", gVirtualKeyboard.repeat ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
        bx                           -= droneW + 6.0;
        gVirtualKeyboard.droneButton  = draw_button(mainArea, (tRectangle){{bx, y}, {droneW, btnH}},
                                                    "Drone", gVirtualKeyboard.drone ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BACKGROUND_GREY);
    }
    y                          += btnH + margin;

    // ── Whites first, then blacks over the top ────────────────────────────
    // Two passes, because a black key overlaps the two whites it sits between and has to be drawn
    // after them. The hit-test walks the blacks first for the same reason.
    double keyY   = y;
    double blackW = whiteW * VKB_BLACK_W_FRAC;
    double blackH = VKB_WHITE_H * VKB_BLACK_H_FRAC;
    double x      = boxX + margin;

    gVirtualKeyboard.whiteCount = 0;
    gVirtualKeyboard.blackCount = 0;

    for (uint32_t i = 0; i < VKB_KEYS_VISIBLE; i++) {
        uint32_t   note = gVirtualKeyboard.firstNote + i;

        if ((note > VKB_NOTE_MAX) || kIsBlack[note % 12]) {
            continue;
        }
        tRectangle key  = {{x, keyY}, {whiteW, VKB_WHITE_H}};

        set_rgb_colour(((int32_t)note == gVirtualKeyboard.noteOn) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_GREY_9);
        render_rectangle_with_border(mainArea, key);

        gVirtualKeyboard.whiteKey[gVirtualKeyboard.whiteCount]  = key;
        gVirtualKeyboard.whiteNote[gVirtualKeyboard.whiteCount] = note;
        gVirtualKeyboard.whiteCount++;

        // C gets its octave number, the one landmark that makes a soft keyboard readable. MIDI 60
        // is C3 in Clavia's numbering (the G2's own display), not the C4 of some other makers.
        if ((note % 12) == 0) {
            char label[8] = {0};

            snprintf(label, sizeof(label), "C%d", (int)(note / 12) - 2);
            set_rgb_colour((tRgb)RGB_GREY_3);
            render_text(mainArea, (tRectangle){{x + 2.0, keyY + VKB_WHITE_H + 2.0}, {BLANK_SIZE, textH}}, label);
        }
        x                                                      += whiteW;
    }

    // Blacks are positioned from the white that precedes them, so they land on the joins rather
    // than on a count of semitones — which would drift across the two- and three-black groups.
    for (uint32_t w = 0; w < gVirtualKeyboard.whiteCount; w++) {
        uint32_t   note  = gVirtualKeyboard.whiteNote[w] + 1;

        if ((note > VKB_NOTE_MAX) || !kIsBlack[note % 12]) {
            continue;   // E and B have no black above them
        }

        if (note >= (gVirtualKeyboard.firstNote + VKB_KEYS_VISIBLE)) {
            continue;   // past the right-hand edge of the span
        }
        tRectangle white = gVirtualKeyboard.whiteKey[w];
        tRectangle key   = {
            {white.coord.x + white.size.w - (blackW / 2.0), keyY}, {blackW, blackH}
        };

        set_rgb_colour(((int32_t)note == gVirtualKeyboard.noteOn) ? (tRgb)RGB_GREEN_ON : (tRgb)RGB_BLACK);
        render_rectangle(mainArea, key);

        gVirtualKeyboard.blackKey[gVirtualKeyboard.blackCount]  = key;
        gVirtualKeyboard.blackNote[gVirtualKeyboard.blackCount] = note;
        gVirtualKeyboard.blackCount++;
    }

    // ── The sounding note's black dot, as the manual describes ────────────
    if (gVirtualKeyboard.noteOn >= 0) {
        tRectangle key   = {0};
        bool       found = false;
        bool       black = false;

        for (uint32_t b = 0; (b < gVirtualKeyboard.blackCount) && !found; b++) {
            if ((int32_t)gVirtualKeyboard.blackNote[b] == gVirtualKeyboard.noteOn) {
                key   = gVirtualKeyboard.blackKey[b];
                black = true;
                found = true;
            }
        }

        for (uint32_t w = 0; (w < gVirtualKeyboard.whiteCount) && !found; w++) {
            if ((int32_t)gVirtualKeyboard.whiteNote[w] == gVirtualKeyboard.noteOn) {
                key   = gVirtualKeyboard.whiteKey[w];
                found = true;
            }
        }

        if (found) {
            // Near the bottom of the key, where a finger would be, and white on a black key so it
            // shows at all.
            set_rgb_colour(black ? (tRgb)RGB_GREY_9 : (tRgb)RGB_BLACK);
            render_circle_part(mainArea,
                               (tCoord){key.coord.x + (key.size.w / 2.0), key.coord.y + key.size.h - 10.0},
                               3.5, 10, 0, 10);
        }
    }
}

// ─── Mouse ───────────────────────────────────────────────────────────────────

// The note under the cursor, or -1. Blacks first: they overlap the whites they sit between, so a
// click in the overlap belongs to the black key, exactly as on a real keyboard.
static int32_t note_at(tCoord coord) {
    for (uint32_t b = 0; b < gVirtualKeyboard.blackCount; b++) {
        if (within_rectangle(coord, gVirtualKeyboard.blackKey[b])) {
            return (int32_t)gVirtualKeyboard.blackNote[b];
        }
    }

    for (uint32_t w = 0; w < gVirtualKeyboard.whiteCount; w++) {
        if (within_rectangle(coord, gVirtualKeyboard.whiteKey[w])) {
            return (int32_t)gVirtualKeyboard.whiteNote[w];
        }
    }

    return -1;
}

bool handle_virtual_keyboard_mouse(tCoord coord, tMouseButton mouseButton) {
    if (!gVirtualKeyboard.active) {
        return false;
    }

    if (mouseButton == mouseButtonLeftDown) {
        if (within_rectangle(coord, gVirtualKeyboard.close)) {
            gVirtualKeyboard.closePressed = true;
        } else {
            int32_t note = note_at(coord);

            if (note >= 0) {
                // lastNote is what Repeat re-strikes — "the LAST PLAYED note" — so it follows the
                // keyboard even while a repeat is already running, and the repeat moves with it.
                gVirtualKeyboard.lastNote     = note;
                gVirtualKeyboard.nextRepeatAt = (glfwGetTime() * 1000.0) + VKB_REPEAT_MS;
                set_sounding_note(note);   // sustains until mouse-up, as the manual describes
            }
        }
    }

    if (mouseButton == mouseButtonLeftUp) {
        bool wasClosePressed = gVirtualKeyboard.closePressed;

        gVirtualKeyboard.closePressed = false;

        // The key is no longer held, so the note goes off — UNLESS Drone is engaged, which is
        // exactly what that button means: "make the next played note start sounding infinitely".
        // Repeat holds the note on too, since it is about to re-strike it anyway and releasing
        // here would just make the first gap longer than the rest.
        if (!gVirtualKeyboard.drone && !gVirtualKeyboard.repeat) {
            set_sounding_note(-1);
        }

        if (wasClosePressed && within_rectangle(coord, gVirtualKeyboard.close)) {
            close_virtual_keyboard_panel();
        } else if (within_rectangle(coord, gVirtualKeyboard.octaveDown)) {
            scroll_by(-12);
        } else if (within_rectangle(coord, gVirtualKeyboard.noteDown)) {
            scroll_by(-1);
        } else if (within_rectangle(coord, gVirtualKeyboard.noteUp)) {
            scroll_by(1);
        } else if (within_rectangle(coord, gVirtualKeyboard.octaveUp)) {
            scroll_by(12);
        } else if (within_rectangle(coord, gVirtualKeyboard.droneButton)) {
            gVirtualKeyboard.drone = !gVirtualKeyboard.drone;

            // Disengaging is how you stop a drone, so it has to silence whatever is running.
            if (!gVirtualKeyboard.drone && !gVirtualKeyboard.repeat) {
                set_sounding_note(-1);
            }
        } else if (within_rectangle(coord, gVirtualKeyboard.repeatButton)) {
            gVirtualKeyboard.repeat = !gVirtualKeyboard.repeat;

            if (gVirtualKeyboard.repeat) {
                gVirtualKeyboard.nextRepeatAt = glfwGetTime() * 1000.0;   // first strike immediately
            } else if (!gVirtualKeyboard.drone) {
                set_sounding_note(-1);
            }
        }
    }
    synthlib_request_redraw();
    return true;
}

// ─── Keyboard ────────────────────────────────────────────────────────────────

bool handle_virtual_keyboard_key(int key, int mods, int action) {
    (void)mods;

    if (!gVirtualKeyboard.active || (action != GLFW_PRESS)) {
        return false;
    }

    switch (key) {
        case GLFW_KEY_ESCAPE:
            close_virtual_keyboard_panel();
            break;

        case GLFW_KEY_LEFT:
            scroll_by(-1);
            break;

        case GLFW_KEY_RIGHT:
            scroll_by(1);
            break;

        case GLFW_KEY_DOWN:
            scroll_by(-12);
            break;

        case GLFW_KEY_UP:
            scroll_by(12);
            break;

        default:
            return false;
    }
    synthlib_request_redraw();
    return true;
}

#ifdef __cplusplus
}
#endif
