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

// stb_image_write is already bundled as a GLFW build dependency — reused here
// (rather than a second PNG library) purely for the backdoor SCREENSHOT command.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../SynthLib/ThirdParty/glfw/deps/stb_image_write.h"
#pragma clang diagnostic pop

#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "defs.h"
#include "synthlibDefs.h"
#include "types.h"
#include "globalVars.h"
#include "dataBase.h"
#include "msgQueue.h"
#include "protocol.h"
#include "moduleResourcesAccess.h"
#include "mouseHandle.h"
#include "menus.h"
#include "selection.h"
#include "soundEngine.h"
#include "cableChain.h"
#include "appMenuBar.h"
#include "contextMenu.h"
#include "utilsGraphics.h"
#include "graphics.h"
#include "splitView.h"
#include "backdoor.h"

// ── Backdoor test-control channel ───────────────────────────────────────────
// A way to drive AND independently verify the running app — load a patch,
// select a slot, dump the module list, or capture a screenshot — without a
// real mouse click or a reliable headless paint event. Ported from SynthEdit's
// proven mechanism (SynthEdit/src/graphics.cpp), adapted to G2's domain.
//
// GATED behind the G2_EDIT_BACKDOOR environment variable: unset (the owner's
// normal double-click launch) => backdoor completely inert AND the idle loop
// keeps glfwWaitEvents()'s full sleep. Set (a test launch from a shell) =>
// the idle loop polls at 10 Hz and a command file is honoured each tick. Unlike
// SynthEdit (sandboxed, needs its container tmp dir) G2-Edit has no App Sandbox,
// so plain /tmp works. Command surface is deliberately narrow and does nothing a
// real mouse click couldn't already do.
//
// Command file (/tmp/g2edit_cmd.txt): one command per file, first line only,
// "<COMMAND> <arg>". Result ("OK\n"/"ERROR: ...\n", or DUMP's own text) is
// written to /tmp/g2edit_result.txt and the command file is deleted, so a
// caller polls for the command file's disappearance to know it's done.
//   LOADFILE <path>   — read_file_into_memory_and_process() (works offline)
//   SLOT <0-3|A-D>    — select the slot the canvas renders
//   COMMS             — "online" or "offline". ASK THIS BEFORE ANY DEV COMMAND YOU INTEND TO TRUST:
//                       every one of them reports OK when the instrument is not listening
//   DUMP              — current slot + every module: type, name, location, col/row
//   LEDDUMP           — live LED and volume-meter values per module, as the renderer reads them.
//                       Poll it to measure a blink RATE, which no screenshot can show
//   PARAMDUMP         — the same modules with their variation-0 PARAMETER and MODE values, plus the
//                       parameter count the patch declared against the one our table gives
//   MENU <bar>[/<item>[/<sub>]] — run a menu item by label (leading substring, case-insensitive,
//                       '/' separated); omit the last level to LIST what that level contains
//   SELECT <VA|FX> <n> — select one module by index; SELECT NONE clears
//   SNDSTATUS         — what the sound engine's status line currently reads
//   SNDDUMP           — the resolved chain, the parameters read, and the peak level since last read
//   NOTE <n>|OFF      — play/release a note on the sound engine (LOCAL engine, not the G2)
//   DEVSET <VA|FX> <index> <param> <value> — as SET, but SENT TO THE G2. This is what lets the
//                       measurement harness step one parameter on the hardware while its audio output
//                       is recorded; SET stays local-only so a rendering test cannot write to a
//                       connected synth by accident.
//   DEVMODE <VA|FX> <index> <mode> <value> — a MODE write to the G2 (the drop-down selectors: the
//                       Reverb's room size, a filter's slope, an oscillator's waveform). Modes travel
//                       on their own wire command, so DEVSET cannot reach them.
//   DEVKNOB <knob 0-119> <VA|FX> <index> <param> — assign a parameter to a patch knob, on the G2 as
//                       well as locally. Needed before the synth's own display can be asked what a
//                       dial reads: an unassigned parameter has nowhere to show itself on the panel.
//   DEVNOTE <note> <vel> on|off — a Virtual Keyboard note to the G2, for a patch that needs a gate
//                       rather than a free-running clock (envelope times, for instance)
//   DEVADDMODULE [VA|FX] <name> [col] [row]  — as ADDMODULE, but created ON THE G2 too. The area
//                       is optional and defaults to VA; FX is the only scripted route into the
//                       effects area, and without it no test patch can have LEDs in both.
//                       OMIT THE ROW to pack it directly under whatever is already in that
//                       column, from the real module heights rather than a guessed gap
//   DEVDELMODULE <VA|FX> <index>     — deletes a module and its cables, on the G2 as well as here
//                       These two exist because every LOCAL-ONLY command can only ever produce a
//                       freshly-loaded patch, and that is the one state where the LED stream is known
//                       to behave. An edit the DEVICE sees is what is needed to chase the LED
//                       ordering fault, and without these it could only be done by hand in the GUI.
//   SELECTADD <VA|FX> <index> — add to the selection rather than replace it
//   MOVESEL <dColumn> <dRow>  — move the whole selection by a grid delta and re-order the column as a
//                       drop does. The only scripted route to a GROUP drop, synthetic drags being
//                       invisible to the app; refuses with "no room" and puts the group back
//   SAVEFILE <path>   — write the current slot to a path (no save panel)
//   SCREENSHOT <path> — synchronous render_frame() then glReadPixels + PNG
//   SCROLL <x> <y>    — scroll the canvas, each 0.0-1.0 of that axis's full travel
//   ZOOM <factor>     — canvas zoom, same 0.25-2.0 range Cmd +/- walks through
//   SPLIT <VA|FX|BALANCE|pixels> — where the Voice/FX divider sits. VA gives the Voice Area the
//                       whole window (the FX area minimised), FX the reverse, BALANCE the
//                       double-arrow's restore; a number is a Voice Area height in pixels. Framing
//                       for a render check: with the divider halfway, a tall module does not fit
//
// SCROLL and ZOOM exist because a synthetic drag doesn't reach the app at all — neither the
// scrollbar thumb nor a dial responds to one — so without them a scripted check can only ever see
// the modules that happen to be on screen at the default zoom.
bool backdoor_enabled(void) {
    static int cached = -1;

    if (cached < 0) {
        const char * v = getenv("G2_EDIT_BACKDOOR");
        cached = (v != NULL && v[0] != '\0') ? 1 : 0;
    }
    return cached == 1;
}

static const char * backdoor_cmd_path(void) {
    return "/tmp/g2edit_cmd.txt";
}

static const char * backdoor_result_path(void) {
    return "/tmp/g2edit_result.txt";
}

// Case-insensitive "does this label contain that text", for the MENU command's label matching.
static bool label_contains(const char * label, const char * wanted) {
    size_t wantedLength = strlen(wanted);
    size_t labelLength  = strlen(label);
    size_t at           = 0;

    if (wantedLength == 0) {
        return false;
    }

    if (wantedLength > labelLength) {
        return false;
    }

    for (at = 0; at <= (labelLength - wantedLength); at++) {
        if (strncasecmp(label + at, wanted, wantedLength) == 0) {
            return true;
        }
    }

    return false;
}

static void backdoor_write_result(const char * text) {
    FILE * f = fopen(backdoor_result_path(), "w");

    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void backdoor_screenshot(const char * path) {
    render_frame(); // synchronous, so the capture always reflects the most recent LOADFILE/SLOT command, not a stale frame

    int       w      = get_render_width();
    int       h      = get_render_height();

    if ((w <= 0) || (h <= 0)) {
        backdoor_write_result("ERROR: zero-size framebuffer\n");
        return;
    }
    uint8_t * pixels = (uint8_t *)malloc((size_t)w * (size_t)h * 3);

    if (!pixels) {
        backdoor_write_result("ERROR: out of memory\n");
        return;
    }

    // Rows come back tightly packed (w*3 bytes) — the alignment that guarantees it, and
    // the sheared-PNG bug that proved it necessary, are inside the backend call.
    if (!render_backend_read_pixels_rgb(0, 0, w, h, pixels)) {
        free(pixels);
        backdoor_write_result("ERROR: frame read-back failed\n");
        return;
    }
    stbi_flip_vertically_on_write(1); // read-back origin is bottom-left; PNGs are top-down

    int       ok     = stbi_write_png(path, w, h, 3, pixels, w * 3);

    free(pixels);
    backdoor_write_result(ok ? "OK\n" : "ERROR: stbi_write_png failed\n");
}

static void backdoor_dump_state(char * out, size_t outMax) {
    size_t         used       = 0;
    const uint32_t locs[]     = {(uint32_t)locationVa, (uint32_t)locationFx};
    const char *   locNames[] = {"VA", "FX"};

    used += (size_t)snprintf(out + used, outMax - used, "OK\nslot=%u\n", (unsigned)gSlot);

    for (uint32_t l = 0; (l < 2) && (used < outMax); l++) {
        for (uint32_t index = 0; (index < MAX_NUM_MODULES) && (used < outMax); index++) {
            tModule * module = get_module_slot(gSlot, locs[l], index);

            if (module == NULL || module->type == 0) {
                continue; // type 0 == empty slot in the sparse per-index store
            }
            used += (size_t)snprintf(out + used, outMax - used,
                                     "loc=%s index=%u type=%u name=\"%s\" col=%u row=%u\n",
                                     locNames[l], (unsigned)index, (unsigned)module->type,
                                     module->name, (unsigned)module->column, (unsigned)module->row);
        }
    }

    // Cables too, so a caller can verify a cable edit — and its undo — without a screenshot.
    // Emitted sorted-by-nothing, i.e. in database order, which a delete-and-recreate reshuffles;
    // compare these as a SET rather than line-by-line.
    for (uint32_t l = 0; (l < 2) && (used < outMax); l++) {
        for (uint32_t index = 0; (index < MAX_NUM_CABLES) && (used < outMax); index++) {
            tCable * cable = get_cable_slot(gSlot, locs[l], index);

            if ((cable == NULL) || !cable->active) {
                continue;
            }
            used += (size_t)snprintf(out + used, outMax - used,
                                     "cable loc=%s from=%u:%u link=%u to=%u:%u colour=%u\n",
                                     locNames[l],
                                     (unsigned)cable->key.moduleFromIndex, (unsigned)cable->key.connectorFromIoCount,
                                     (unsigned)cable->key.linkType,
                                     (unsigned)cable->key.moduleToIndex, (unsigned)cable->key.connectorToIoCount,
                                     (unsigned)cable->colour);
        }
    }
}

// LEDDUMP — the live LED and volume-meter state of every module in the current slot, exactly as
// render_module() reads it. Eyeballing a screenshot cannot answer "which module is stream index 3",
// and a blink RATE is invisible in a still; this reports the numbers the renderer draws from, so a
// caller can poll it and count transitions per module.
//
// leds= is one 0-3 value per LED in ledLocationList order (the order parse_led_data() fills), vols=
// one 0-255 per meter. A module with neither is skipped, which keeps the output to the few modules
// an LED test actually cares about.
static void backdoor_led_dump(void) {
    FILE *         file       = fopen(backdoor_result_path(), "w");

    if (file == NULL) {
        return;
    }
    const uint32_t locs[]     = {(uint32_t)locationVa, (uint32_t)locationFx};
    const char *   locNames[] = {"VA", "FX"};

    // glfwGetTime() rather than get_time_ms(): this file already has glfw3.h, runs on the UI
    // thread, and the caller only needs the samples ORDERED and spaced, not wall-clock.
    fprintf(file, "OK\nslot=%u t=%.3f\n", (unsigned)gSlot, glfwGetTime());

    for (uint32_t l = 0; l < 2; l++) {
        for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
            tModule * module   = get_module_slot(gSlot, locs[l], index);

            if ((module == NULL) || (module->type == 0)) {
                continue; // type 0 == empty slot in the sparse per-index store
            }
            uint32_t  ledCount = module_led_count(module->type) + module_multibit_led_count(module->type);
            uint32_t  volCount = 0;

            switch (gModuleProperties[module->type].volumeType) {
                case volumeTypeMono:
                case volumeTypeCompress:
                case volumeTypeSequencer: volCount = 1;
                    break;
                case volumeTypeStereo:    volCount = 2;
                    break;
                case volumeTypeQuad:      volCount = 4;
                    break;
                case volumeTypeNone:      volCount = 0;
                    break;
            }

            if ((ledCount == 0) && (volCount == 0)) {
                continue;
            }
            fprintf(file, "loc=%s index=%u type=%u name=\"%s\" leds=",
                    locNames[l], (unsigned)index, (unsigned)module->type, module->name);

            for (uint32_t i = 0; (i < ledCount) && (i < MAX_LEDS_PER_MODULE); i++) {
                fprintf(file, "%s%u", (i == 0) ? "" : ",", (unsigned)module->led.value[i]);
            }

            if (ledCount == 0) {
                fprintf(file, "-");
            }
            fprintf(file, " vols=");

            for (uint32_t i = 0; i < volCount; i++) {
                fprintf(file, "%s%u", (i == 0) ? "" : ",", (unsigned)module->volume.value[i]);
            }

            if (volCount == 0) {
                fprintf(file, "-");
            }
            fprintf(file, "\n");
        }
    }

    fclose(file);
}

// PARAMDUMP — every active module in the current slot with its VARIATION 0 parameter values and its
// mode values. DUMP above reports only STRUCTURE (which modules, which cables); this reports
// CONTENTS, which is what auditing paramLocationList's defaultValue column against a reference patch
// needs.
//
// Prints the count the PATCH declared (module->actualParamCount) alongside the count our own table
// gives, so the same dump doubles as a param-count check on any device-authored file.
//
// Written straight to the result file rather than composed in a buffer the way backdoor_dump_state()
// is: a full patch runs to tens of modules by tens of parameters, which overruns any fixed buffer
// worth putting on the stack.
// Tells the instrument about a cable the backdoor just added or removed, exactly as the drag-connect
// and Disconnect paths do. Nothing happens when offline, which is what makes the same script usable
// as an offline layout scratchpad.
static void backdoor_send_cable(const tCableKey * key, uint32_t colour, bool removing) {
    if (!device_ready()) {
        return;
    }
    tMessageContent msg = {0};

    msg.cmd                            = removing ? eMsgCmdDeleteCable : eMsgCmdWriteCable;
    msg.slot                           = gSlot;
    msg.cableData.location             = key->location;
    msg.cableData.moduleFromIndex      = key->moduleFromIndex;
    msg.cableData.connectorFromIoIndex = key->connectorFromIoCount;
    msg.cableData.moduleToIndex        = key->moduleToIndex;
    msg.cableData.connectorToIoIndex   = key->connectorToIoCount;
    msg.cableData.linkType             = key->linkType;
    msg.cableData.colour               = colour;
    msg_send(&gToUsbThread, &msg);
}

static void backdoor_param_dump(void) {
    FILE *         file       = fopen(backdoor_result_path(), "w");

    if (file == NULL) {
        return;
    }
    const uint32_t locs[]     = {(uint32_t)locationVa, (uint32_t)locationFx};
    const char *   locNames[] = {"VA", "FX"};

    fprintf(file, "OK\nslot=%u\n", (unsigned)gSlot);

    for (uint32_t l = 0; l < 2; l++) {
        for (uint32_t index = 0; index < MAX_NUM_MODULES; index++) {
            tModule * module = get_module_slot(gSlot, locs[l], index);

            if ((module == NULL) || (module->type == 0)) {
                continue; // type 0 == empty slot in the sparse per-index store
            }
            fprintf(file, "module loc=%s index=%u type=%u name=\"%s\" filecount=%u tablecount=%u params:",
                    locNames[l], (unsigned)index, (unsigned)module->type, module->name,
                    (unsigned)module->actualParamCount, (unsigned)module_param_count(module->type));

            for (uint32_t p = 0; (p < module->actualParamCount) && (p < MAX_NUM_PARAMETERS); p++) {
                fprintf(file, " %u", (unsigned)module->param[0][p].value);
            }

            fprintf(file, "\nmodes loc=%s index=%u count=%u:", locNames[l], (unsigned)index, (unsigned)module->modeCount);

            for (uint32_t m = 0; (m < module->modeCount) && (m < MAX_NUM_MODES); m++) {
                fprintf(file, " %u", (unsigned)module->mode[m].value);
            }

            fprintf(file, "\n");
        }
    }

    fclose(file);
}

// A cable end is addressed by its I/O index — "output 2", "input 0" — counting only connectors of
// that direction. That is how the protocol expresses it, how tCableKey stores it, and how the DUMP
// command above prints it. A module's connector array is in declaration order with both directions
// interleaved, so turning one into the other is a walk. -1 if the module has no such connector.
static int32_t backdoor_connector_for_io_index(tModule * module, bool wantOutput, uint32_t ioIndex) {
    uint32_t count = module_connector_count(module->type);
    uint32_t seen  = 0;

    for (uint32_t i = 0; i < count; i++) {
        if ((module->connector[i].dir == connectorDirOut) == wantOutput) {
            if (seen == ioIndex) {
                return (int32_t)i;
            }
            seen++;
        }
    }

    return -1;
}

// The next free row in a column, worked out from the heights gModuleProperties already carries.
// A scripted patch used to have to guess its own spacing, and a guess is either loose - the envelope
// measurement patch sat at rows 0/6/11/18 where those modules are 4/2/5/2 tall, so it wasted eight
// rows and needed scrolling to see - or too tight, which lands one module inside another. Placement
// was only ever a guess because the caller cannot see the heights; here they are.
static uint32_t backdoor_next_free_row(uint32_t slot, uint32_t location, uint32_t column) {
    uint32_t next = 0;

    for (uint32_t i = 0; i < MAX_NUM_MODULES; i++) {
        tModule * walk   = get_module_slot(slot, location, i);

        if ((walk == NULL) || !walk->active || (walk->column != column)) {
            continue;
        }
        uint32_t  bottom = walk->row + gModuleProperties[walk->type].height;

        if (bottom > next) {
            next = bottom;
        }
    }

    return next;
}

// Shared argument parsing for CABLE and DELCABLE: "<VA|FX> <from>:<out> <to>:<in> [link=<0|1>]",
// deliberately the same shape DUMP prints, so a dumped cable can be pasted straight back as a
// command. link defaults to 1 (the from-end is an output); 0 is a fan-out from an input connector.
static bool backdoor_parse_cable(const char * arg, tCableKey * key, char * err, size_t errMax) {
    char         loc[8]    = {0};
    uint32_t     fromIndex = 0;
    uint32_t     fromIo    = 0;
    uint32_t     toIndex   = 0;
    uint32_t     toIo      = 0;
    const char * linkText  = NULL;
    uint32_t     link      = (uint32_t)cableLinkTypeFromOutput;

    if (sscanf(arg, "%7s %u:%u %u:%u", loc, &fromIndex, &fromIo, &toIndex, &toIo) != 5) {
        snprintf(err, errMax, "ERROR: expected '<VA|FX> <from>:<out> <to>:<in> [link=<0|1>]'\n");
        return false;
    }
    linkText                  = strstr(arg, "link=");

    if ((linkText != NULL) && (sscanf(linkText, "link=%u", &link) != 1)) {
        snprintf(err, errMax, "ERROR: link must be 0 or 1\n");
        return false;
    }

    if (link > (uint32_t)cableLinkTypeFromOutput) {
        snprintf(err, errMax, "ERROR: link must be 0 or 1\n");
        return false;
    }
    key->slot                 = gSlot;
    key->location             = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
    key->moduleFromIndex      = fromIndex;
    key->connectorFromIoCount = fromIo;
    key->linkType             = link;
    key->moduleToIndex        = toIndex;
    key->connectorToIoCount   = toIo;
    return true;
}

// An input connector takes at most one incoming cable — the same invariant cable-drag creation
// enforces before it commits. Checked here too, because the device silently keeps whichever it
// likes when told otherwise.
// Is this connector already serving as the input end of some cable? An input takes exactly one
// cable, so this is what stops a scripted CABLE from stacking a second one on top of an existing
// connection - which looks like nothing at all on screen, because the two are drawn along the same
// path, and only shows itself when you pull one off and the sound stays.
//
// BOTH ENDS OF A cableLinkTypeFromInput CABLE ARE INPUTS (see cableChain.h): that link type is the
// G2's input-to-input daisy chain, so its from-end is an input just as much as its to-end is. This
// used to test the to-end alone, so an input already spoken for as the FROM-end of such a chain
// read as free and got a second cable.
static bool backdoor_connector_is_input_end(const tCableKey * cableKey, uint32_t moduleIndex, uint32_t ioCount) {
    if ((cableKey->moduleToIndex == moduleIndex) && (cableKey->connectorToIoCount == ioCount)) {
        return true;
    }
    return (cableKey->linkType == (uint32_t)cableLinkTypeFromInput)
           && (cableKey->moduleFromIndex == moduleIndex)
           && (cableKey->connectorFromIoCount == ioCount);
}

static bool backdoor_input_is_taken(const tCableKey * key) {
    for (uint32_t i = 0; i < MAX_NUM_CABLES; i++) {
        tCable * cable = get_cable_slot(key->slot, key->location, i);

        if ((cable == NULL) || !cable->active) {
            continue;
        }

        // The new cable's to-end is always an input; its from-end is one too when it is itself an
        // input-to-input link, and either would be a second cable on an already-occupied input.
        if (backdoor_connector_is_input_end(&cable->key, key->moduleToIndex, key->connectorToIoCount)) {
            return true;
        }

        if (  (key->linkType == (uint32_t)cableLinkTypeFromInput)
           && backdoor_connector_is_input_end(&cable->key, key->moduleFromIndex, key->connectorFromIoCount)) {
            return true;
        }
    }

    return false;
}

static void backdoor_dispatch(const char * cmd, const char * arg) {
    if (strcmp(cmd, "LOADFILE") == 0) {
        if (arg[0] == '\0') {
            backdoor_write_result("ERROR: expected 'LOADFILE <path>'\n");
            return;
        }
        read_file_into_memory_and_process(arg);
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "SLOT") == 0) {
        uint32_t slot = 0;

        if (arg[0] >= 'A' && arg[0] <= 'D') {
            slot = (uint32_t)(arg[0] - 'A');
        } else if (arg[0] >= 'a' && arg[0] <= 'd') {
            slot = (uint32_t)(arg[0] - 'a');
        } else if (sscanf(arg, "%u", &slot) != 1 || slot > 3) {
            backdoor_write_result("ERROR: expected 'SLOT <0-3|A-D>'\n");
            return;
        }
        gSlot                 = slot;
        gPatchParamsEdit.slot = slot; // patch-params panel tracks its own slot copy — keep both in step
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "NEWPATCH") == 0) {
        // Clears the current slot's canvas — AND the instrument's, when there is one. It used to
        // clear only the local database, which quietly left the G2 holding the previous patch: a test
        // that built a "clean" patch on the device inherited the old one's cables, and the LEDs it
        // then reported were correct for a patch nobody could see. Divergence between our copy and
        // the edit buffer is the one thing a test harness must not introduce.
        database_delete_modules_by_slot(gSlot);
        database_delete_cables_by_slot(gSlot);
        gLocation = (uint32_t)locationVa;

        if (device_ready()) {
            tMessageContent msg = {0};

            msg.cmd  = eMsgCmdNewPatch;
            msg.slot = gSlot;
            msg_send(&gToUsbThread, &msg);
        }
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if ((strcmp(cmd, "ADDMODULE") == 0) || (strcmp(cmd, "DEVADDMODULE") == 0)) {
        // BOTH SPELLINGS REACH THE INSTRUMENT. ADDMODULE was local-only, which meant a scripted patch
        // and the G2's edit buffer could drift apart without anything saying so — and every LED, knob
        // and cable index the device reports is relative to ITS copy. DEVADDMODULE remains as a
        // synonym so existing scripts keep working.
        bool         toDevice = true;

        // ADDMODULE [VA|FX] <name> [col] [row] — name matches gModuleProperties[].name
        // (e.g. "Mix4-1C"). The area is optional and defaults to VA, which is what this command did
        // before it existed, so every existing script is unaffected.
        //
        // THE AREA ARGUMENT IS NOT A CONVENIENCE. Until it was added there was no scripted way to
        // put a module in the FX area at all, and that shows in the test corpus: all 18 files in
        // PatchTestFiles have their LED-bearing modules in VA and none in FX. The 0x39 stream's
        // index space is the two areas concatenated, so with one of them empty both possible area
        // orderings give the same answer and every LED test we have passes either way. Settling
        // which order the instrument really uses needs LEDs in both areas — see todo.txt.
        char         name[64] = {0};
        uint32_t     col      = 0;
        uint32_t     row      = 0;
        uint32_t     area     = (uint32_t)locationVa;
        const char * rest     = arg;
        char         first[8] = {0};

        if (sscanf(rest, "%7s", first) == 1) {
            if ((strcasecmp(first, "VA") == 0) || (strcasecmp(first, "FX") == 0)) {
                area  = (strcasecmp(first, "FX") == 0) ? (uint32_t)locationFx : (uint32_t)locationVa;
                rest += strlen(first);

                while ((*rest == ' ') || (*rest == '\t')) {
                    rest++;
                }
            }
        }
        int          given    = sscanf(rest, "%63s %u %u", name, &col, &row);

        if (given < 1) {
            backdoor_write_result("ERROR: expected 'ADDMODULE [VA|FX] <name> [col] [row]'\n");
            return;
        }
        tModuleType  found    = (tModuleType)0;

        for (uint32_t t = 1; t < (uint32_t)moduleTypeMax; t++) {
            if (strcmp(gModuleProperties[t].name, name) == 0) {
                found = (tModuleType)t;
                break;
            }
        }

        if (found == (tModuleType)0) {
            char msg[128];

            snprintf(msg, sizeof(msg), "ERROR: no module named '%s'\n", name);
            backdoor_write_result(msg);
            return;
        }
        gLocation = area;

        // OMIT THE ROW AND THEY PACK. With a row given, create_module_at() still ends in
        // shift_modules_down(), so an explicit row landing on top of something pushes it out of the
        // way rather than drawing garbled - but only an omitted row is placed tight against whatever
        // is already in the column.
        if (given < 3) {
            row = backdoor_next_free_row(gSlot, gLocation, col);
        }
        // DEVADDMODULE syncs to the instrument where ADDMODULE stays local — the DEV prefix means the
        // same thing here as it does on DEVSET and DEVMODE. It exists because the LOCAL-ONLY commands
        // can only ever produce a freshly-loaded patch, and a freshly-loaded patch is exactly the
        // state where the LED stream is known to behave: reproducing the ordering fault needs edits
        // the DEVICE sees, which until now meant driving the GUI by hand.
        int32_t idx = create_module_at(found, col, row, toDevice);

        synthlib_request_redraw();
        backdoor_write_result((idx < 0) ? "ERROR: location full\n" : "OK\n");
    } else if (strcmp(cmd, "DEVDELMODULE") == 0) {
        // DEVDELMODULE <VA|FX> <index> — deletes on the instrument as well as here, cables and all,
        // which is what the module right-click menu's Delete does.
        char       area[8]     = {0};
        uint32_t   moduleIndex = 0;

        if (sscanf(arg, "%7s %u", area, &moduleIndex) != 2) {
            backdoor_write_result("ERROR: expected 'DEVDELMODULE <VA|FX> <index>'\n");
            return;
        }
        uint32_t   location    = (strcasecmp(area, "FX") == 0) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModuleKey key         = {gSlot, location, moduleIndex};

        if (get_module(key) == NULL) {
            backdoor_write_result("ERROR: no such module\n");
            return;
        }
        delete_module_and_cables(key);
        selection_clear();
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if ((strcmp(cmd, "CABLE") == 0) || (strcmp(cmd, "DELCABLE") == 0)) {
        // CABLE / DELCABLE <VA|FX> <from>:<out> <to>:<in> [link=<0|1>]
        //
        // SENT TO THE INSTRUMENT, one message per edit — the same eMsgCmdWriteCable/eMsgCmdDeleteCable
        // the drag-connect path sends, so a scripted cable is indistinguishable from a drawn one.
        //
        // These used to be local-only, with PUSH afterwards to send the slot in one versioned
        // command. That is still the right shape for a BULK run (see the note above
        // send_whole_patch() in menus.c: a burst of per-entry commands can race the G2's asynchronous
        // version notification), and PUSH is still there for it. But local-only as the DEFAULT let a
        // script and the edit buffer drift apart silently, and every index the device reports back —
        // LED slots above all — is relative to ITS copy, not ours. A harness that can lie about what
        // the instrument holds is worse than one that is occasionally slow.
        bool      removing = (cmd[0] == 'D');
        tCableKey key      = {0};
        char      msg[160] = {0};

        if (!backdoor_parse_cable(arg, &key, msg, sizeof(msg))) {
            backdoor_write_result(msg);
            return;
        }

        if (removing) {
            if (get_cable(key) == NULL) {
                backdoor_write_result("ERROR: no such cable\n");
                return;
            }
            delete_cable(key);
            backdoor_send_cable(&key, 0, true);
            // A topology change re-assesses up-rate across the slot, exactly as the drag path does
            // (canvasDrag.c) — REMOVING a cable can de-rate a module just as adding one promotes it.
            update_module_up_rates();
            synthlib_request_redraw();
            backdoor_write_result("OK\n");
            return;
        }
        tModule * fromModule    = get_module_slot(key.slot, key.location, key.moduleFromIndex);
        tModule * toModule      = get_module_slot(key.slot, key.location, key.moduleToIndex);

        if ((fromModule == NULL) || (fromModule->type == 0) || (toModule == NULL) || (toModule->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }
        int32_t   fromConnector = backdoor_connector_for_io_index(fromModule, key.linkType == (uint32_t)cableLinkTypeFromOutput, key.connectorFromIoCount);
        int32_t   toConnector   = backdoor_connector_for_io_index(toModule, false, key.connectorToIoCount);

        if ((fromConnector < 0) || (toConnector < 0)) {
            backdoor_write_result("ERROR: connector index out of range for that module type\n");
            return;
        }

        if (backdoor_input_is_taken(&key)) {
            backdoor_write_result("ERROR: that input already has a cable\n");
            return;
        }
        tCable    cable         = {0};

        // The cable inherits the from-connector's CURRENT colour, up-rate promotion included — the
        // same rule cable-drag creation follows, so a scripted patch looks like a drawn one.
        cable.colour = (uint32_t)cable_colour_for_connector_type(
            effective_connector_type(fromModule->connector[fromConnector].type, fromModule->upRate));
        write_cable(key, &cable);
        backdoor_send_cable(&key, cable.colour, false);
        // THE COLOUR ABOVE ONLY GUESSES, and two separate things correct it — the drag path does both,
        // and until now this did only the second.
        //
        // FIRST, RE-DERIVE THE CHAIN'S COLOUR ACROSS THE WHOLE TREE, which is what maintains the
        // invariant the guess cannot: every cable in a chain carries ONE colour, its source output's,
        // or WHITE when the chain has no source at all. A scripted fan-out inherited each cable's
        // colour from its own from-connector and so could paint one chain two colours, and a scripted
        // input-to-input link — which is sourceless and must come out WHITE (the manual's
        // "non-functional input-to-input connections") — came out whatever the from-input happened to
        // be. Neither showed up in the measurement patches built so far, because every cable in them
        // takes its colour from its own source, which is exactly the sort of luck that stops being
        // true the first time a patch fans out.
        //
        // The to-end is always an input, whichever link type this is, so the node needs no database
        // lookup — the same construction canvasDrag.c uses. cable_chain_apply_colour() sends
        // eMsgCmdSetCableColour per cable it actually changes, so the instrument follows; it is a
        // RECOLOUR and not a write, which is what stops the G2 holding each cable twice.
        cable_chain_recolour(key.slot, key.location,
                             (tCableNode){key.moduleToIndex, key.connectorToIoCount, false});

        // SECOND, re-assess up-rate across the slot. Feeding an audio output into a multi-bandwidth
        // (Control/Logic) input promotes the DESTINATION module to audio rate, which repaints the
        // cables leaving it and changes the rate the G2 runs it at. Before 2026-08-24 a scripted patch
        // never did this, so backdoor-built patches drew a promoted module's outputs in the wrong
        // colour AND left the instrument running it at control rate, with eMsgCmdSetModuleUpRate never
        // sent. Ordered after the recolour, as in canvasDrag.c.
        update_module_up_rates();
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "PUSH") == 0) {
        // Sends the current slot to the device as one whole-patch write, which is what makes a run of
        // local CABLE/ADDMODULE/SET edits real. One command, one patch version, nothing to race.
        tMessageContent msg = {0};

        msg.cmd  = eMsgCmdWritePatch;
        msg.slot = gSlot;
        msg_send(&gToUsbThread, &msg);
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "SET") == 0) {
        // SET <VA|FX> <index> <param> <value> — set a param's value in the
        // current slot, LOCAL-ONLY (no device write); for inspecting how a
        // param renders across its range.
        char      loc[8]   = {0};
        uint32_t  index    = 0;
        uint32_t  param    = 0;
        uint32_t  value    = 0;

        if (sscanf(arg, "%7s %u %u %u", loc, &index, &param, &value) != 4) {
            backdoor_write_result("ERROR: expected 'SET <VA|FX> <index> <param> <value>'\n");
            return;
        }
        uint32_t  location = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModule * module   = get_module_slot(gSlot, location, index);

        if ((module == NULL) || (module->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }

        if (param >= MAX_NUM_PARAMETERS) {
            backdoor_write_result("ERROR: param index out of range\n");
            return;
        }
        module->param[gPatchDescr[gSlot].activeVariation][param].value = value;
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "DEVSET") == 0) {
        // DEVSET <VA|FX> <index> <param> <value> — like SET, but SENDS THE CHANGE TO THE G2 as a dial
        // drag would, instead of only touching the local copy.
        //
        // This exists for the measurement harness: characterising a module means stepping one of its
        // parameters over its range while recording the device's audio output, and that only works if
        // the hardware actually follows. SET stays local-only for its own purpose (seeing how a value
        // RENDERS), and the two are deliberately separate commands so a rendering test can never
        // write to a connected synth by accident.
        //
        // Nothing here is destructive: this is a live parameter edit, exactly what the canvas does on
        // every drag, and it does not store to flash.
        char      loc[8]     = {0};
        uint32_t  index      = 0;
        uint32_t  param      = 0;
        uint32_t  value      = 0;

        if (sscanf(arg, "%7s %u %u %u", loc, &index, &param, &value) != 4) {
            backdoor_write_result("ERROR: expected 'DEVSET <VA|FX> <index> <param> <value>'\n");
            return;
        }
        uint32_t  location   = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModule * module     = get_module_slot(gSlot, location, index);

        if ((module == NULL) || (module->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }
        // VALIDATED AGAINST THE MODULE'S OWN PARAM COUNT, not against the array bound. MAX_NUM_PARAMETERS
        // is the size of the store, not the number this module has, and a write past the real count goes
        // out on the wire, gets dropped by the G2, and reports OK — the local copy has already changed,
        // so nothing anywhere says the device disagreed.
        //
        // THAT COST THREE MEASUREMENT RUNS. The Reverb's Small/Medium/Large/Hall selector is a MODE, and
        // a sweep that drove it as `DEVSET <index> 4` produced three files of the same room with no
        // complaint from anything. The lengths only came out identical because they genuinely were.
        uint32_t  paramCount = module_param_count(module->type);

        if (param >= paramCount) {
            char     msg[160];
            uint32_t modeCount = module->modeCount;

            snprintf(msg, sizeof(msg), "ERROR: param %u out of range (module has %u)%s\n",
                     (unsigned)param, (unsigned)paramCount,
                     (modeCount > 0) ? " — a drop-down selector is a MODE: try DEVMODE" : "");
            backdoor_write_result(msg);
            return;
        }
        uint32_t  variation  = gPatchDescr[gSlot].activeVariation;

        module->param[variation][param].value = (uint8_t)value;
        send_param_value(gSlot, module->key, param, variation, value);
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "DEVKNOB") == 0) {
        // DEVKNOB <knob 0-119> <VA|FX> <index> <param> — assign a parameter to one of the patch's
        // knobs, on the G2 as well as locally. The same thing the canvas's Assign Knob menu does.
        //
        // This is what makes a hardware reading possible at all: a parameter the panel has not been
        // pointed at cannot be shown on the synth's display, so a question of the form "what does
        // this dial actually read" needs the assignment before it needs the value.
        //
        // Knob numbering is the 0-119 the patch stores: 24 to a page, 8 to a bank within it, so
        // knob 0 is page 1 bank A position 1 — the first knob of the first page.
        char            loc[8]    = {0};
        uint32_t        knobIndex = 0;
        uint32_t        index     = 0;
        uint32_t        param     = 0;

        if (sscanf(arg, "%u %7s %u %u", &knobIndex, loc, &index, &param) != 4) {
            backdoor_write_result("ERROR: expected 'DEVKNOB <knob 0-119> <VA|FX> <index> <param>'\n");
            return;
        }

        if (knobIndex >= MAX_NUM_KNOBS) {
            backdoor_write_result("ERROR: knob index out of range (0-119)\n");
            return;
        }
        uint32_t        location  = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModule *       module    = get_module_slot(gSlot, location, index);

        if ((module == NULL) || (module->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }

        if (param >= module_param_count(module->type)) {
            backdoor_write_result("ERROR: param out of range for that module type\n");
            return;
        }
        tMessageContent msg       = {0};

        // Free the knob first if something is already on it, exactly as the menu does - the G2 keeps
        // one parameter per knob and a bare assign over an occupied one is not the way to replace it.
        if (gKnobArray[gSlot].knob[knobIndex].assigned) {
            msg.cmd                        = eMsgCmdDeassignKnob;
            msg.slot                       = gSlot;
            msg.knobDeassignData.knobIndex = knobIndex;
            msg_send(&gToUsbThread, &msg);
            memset(&msg, 0, sizeof(msg));
        }
        gKnobArray[gSlot].knob[knobIndex].assigned    = true;
        gKnobArray[gSlot].knob[knobIndex].location    = location;
        gKnobArray[gSlot].knob[knobIndex].moduleIndex = index;
        gKnobArray[gSlot].knob[knobIndex].isLed       = 0;
        gKnobArray[gSlot].knob[knobIndex].paramIndex  = param;

        msg.cmd                                       = eMsgCmdAssignKnob;
        msg.slot                                      = gSlot;
        msg.knobAssignData.moduleKey                  = module->key;
        msg.knobAssignData.paramIndex                 = param;
        msg.knobAssignData.knobIndex                  = knobIndex;
        msg_send(&gToUsbThread, &msg);

        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "DEVMODE") == 0) {
        // DEVMODE <VA|FX> <index> <mode> <value> — a MODE write to the G2, the companion to DEVSET.
        //
        // Modes are the drop-down selectors, and they travel on their own wire command rather than as
        // parameters: the Reverb's Small/Medium/Large/Hall is a mode, as are a filter's slope and an
        // oscillator's waveform. Measuring across those settings is exactly what the harness is for, so
        // without this the most valuable sweep of all — the four reverb room sizes, which are data that
        // exists nowhere else — could not be driven at all.
        char      loc[8]   = {0};
        uint32_t  index    = 0;
        uint32_t  mode     = 0;
        uint32_t  value    = 0;

        if (sscanf(arg, "%7s %u %u %u", loc, &index, &mode, &value) != 4) {
            backdoor_write_result("ERROR: expected 'DEVMODE <VA|FX> <index> <mode> <value>'\n");
            return;
        }
        uint32_t  location = ((loc[0] == 'F') || (loc[0] == 'f')) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModule * module   = get_module_slot(gSlot, location, index);

        if ((module == NULL) || (module->type == 0)) {
            backdoor_write_result("ERROR: no module at that loc/index\n");
            return;
        }

        if (mode >= module->modeCount) {
            char msg[96];

            snprintf(msg, sizeof(msg), "ERROR: mode %u out of range (module has %u)\n",
                     (unsigned)mode, (unsigned)module->modeCount);
            backdoor_write_result(msg);
            return;
        }
        module->mode[mode].value = (uint8_t)value;
        send_mode_value(gSlot, module->key, mode, value);
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "DEVNOTE") == 0) {
        // DEVNOTE <note> <velocity> on|off — a Virtual Keyboard note to the DEVICE, for exciting a
        // patch that needs a gate rather than a free-running clock (envelope times, for instance).
        char            state[8] = {0};
        uint32_t        note     = 0;
        uint32_t        velocity = 0;

        if (sscanf(arg, "%u %u %7s", &note, &velocity, state) != 3) {
            backdoor_write_result("ERROR: expected 'DEVNOTE <note> <velocity> on|off'\n");
            return;
        }

        if ((note > 127) || (velocity > 127)) {
            backdoor_write_result("ERROR: note and velocity are 0-127\n");
            return;
        }
        tMessageContent msg      = {0};

        msg.cmd                   = eMsgCmdPlayNote;
        msg.playNoteData.note     = note;
        msg.playNoteData.velocity = velocity;
        msg.playNoteData.on       = (state[0] == 'o') && (state[1] == 'n');
        msg_send(&gToUsbThread, &msg);
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "COMMS") == 0) {
        // ONLINE OR OFFLINE, ASKED DIRECTLY — and it exists because nothing else here can tell you.
        // Every DEV command reports OK whether or not the instrument is listening: they update the
        // local database first and post to the USB thread second, and that post is a no-op when
        // nothing is connected. So a measurement sweep driven at an offline editor runs to
        // completion, writes its files, and records SILENCE, with every step along the way saying
        // OK. That happened on 2026-08-24 and cost a capture that looked perfectly valid.
        //
        // The obvious tell is missing too: the top bar says "Offline" both when no G2 is plugged in
        // and when another copy of the editor holds the USB claim.
        char text[64];

        snprintf(text, sizeof(text), "OK\ncomms=%s\n",
                 device_ready() ? "online" : "offline");
        backdoor_write_result(text);
    } else if (strcmp(cmd, "DUMP") == 0) {
        char dump[16384];

        backdoor_dump_state(dump, sizeof(dump));
        backdoor_write_result(dump);
    } else if (strcmp(cmd, "LEDDUMP") == 0) {
        backdoor_led_dump();
    } else if (strcmp(cmd, "PARAMDUMP") == 0) {
        backdoor_param_dump();
    } else if (strcmp(cmd, "MENU") == 0) {
        // MENU <bar>[/<item>[/<subitem>]] — runs a menu item by label without going near the mouse.
        // Labels are matched case-insensitively on a leading substring and separated by '/', so
        // 'MENU Exp/Audio Device/QU-24' reaches into a flyout. Any trailing level omitted, the
        // deepest menu reached is LISTED instead of clicked, which is how a test discovers what is
        // there. Driving these by screen coordinates meant re-deriving them whenever a window moved.
        char        part[3][64] = {0};
        uint32_t    partCount   = 0;
        uint32_t    b           = 0;
        tMenuItem * items       = NULL;

        {
            const char * p = arg;

            while ((partCount < 3) && (*p != '\0')) {
                const char * slash  = strchr(p, '/');
                size_t       length = (slash != NULL) ? (size_t)(slash - p) : strlen(p);

                while ((length > 0) && (*p == ' ')) {
                    p++;
                    length--;
                }

                while ((length > 0) && (p[length - 1] == ' ')) {
                    length--;
                }

                if (length >= sizeof(part[0])) {
                    length = sizeof(part[0]) - 1;
                }
                memcpy(part[partCount], p, length);
                part[partCount][length] = '\0';
                partCount++;

                if (slash == NULL) {
                    break;
                }
                p                       = slash + 1;
            }
        }

        if (partCount == 0) {
            backdoor_write_result("ERROR: expected 'MENU <bar>[/<item>[/<subitem>]]'\n");
            return;
        }

        for (b = 0; gAppMenuBar[b].label != NULL; b++) {
            if (label_contains(gAppMenuBar[b].label, part[0]) == true) {
                break;
            }
        }

        if (gAppMenuBar[b].label == NULL) {
            backdoor_write_result("ERROR: no such menu\n");
            return;
        }
        // Populates gContextMenu with the items that menu would show right now, which is what makes
        // state-dependent labels ("Disable Sound Engine") matchable.
        gAppMenuBar[b].open((tCoord){0.0, 0.0});
        items = (gContextMenu.depth > 0) ? gContextMenu.frame[0].items : NULL;

        {
            uint32_t level = 1;

            // Walk down through the named levels. Every level but the last must be a flyout.
            while ((level < partCount) && (items != NULL)) {
                uint32_t i     = 0;
                bool     found = false;

                for (i = 0; items[i].label != NULL; i++) {
                    // Matched ANYWHERE in the label, not just at the front: menu labels carry a
                    // leading "* " marker for the current selection, so a leading-substring match
                    // could never name the thing being selected.
                    if (label_contains(items[i].label, part[level]) == false) {
                        continue;
                    }
                    found = true;

                    if (level == (partCount - 1)) {
                        // The deepest named level: click it, unless it is itself a flyout, in which
                        // case descend so the listing below shows what it contains.
                        if (items[i].subMenu != NULL) {
                            items = items[i].subMenu;
                            break;
                        }
                        {
                            void (*action)(int index) = items[i].action;

                            // action() callbacks read gContextMenu.items[index].param, so point that
                            // at the array the item lives in — the same thing a real click does.
                            gContextMenu.items = items;

                            if (action == NULL) {
                                close_context_menu();
                                backdoor_write_result("ERROR: item is disabled\n");
                                return;
                            }
                            action((int)i);
                            close_context_menu();
                            synthlib_request_redraw();
                            backdoor_write_result("OK\n");
                            return;
                        }
                    }

                    if (items[i].subMenu == NULL) {
                        close_context_menu();
                        backdoor_write_result("ERROR: that item has no submenu\n");
                        return;
                    }
                    items = items[i].subMenu;
                    break;
                }

                if (found == false) {
                    close_context_menu();
                    backdoor_write_result("ERROR: no such item\n");
                    return;
                }
                level++;
            }
        }

        // Nothing left to click: list what the level we reached contains.
        {
            char     list[2048] = {0};
            size_t   used       = 0;
            uint32_t i          = 0;

            used += (size_t)snprintf(list + used, sizeof(list) - used, "OK\n");

            for (i = 0; (items != NULL) && (items[i].label != NULL) && (used < sizeof(list)); i++) {
                used += (size_t)snprintf(list + used, sizeof(list) - used, "%s%s\n",
                                         items[i].label, (items[i].subMenu != NULL) ? " >" : "");
            }

            close_context_menu();
            backdoor_write_result(list);
        }
    } else if (strcmp(cmd, "SELECTADD") == 0) {
        // SELECTADD <VA|FX> <index> — adds to the selection instead of replacing it, so a script can
        // build the multiple selection that MOVESEL below needs.
        char      locName[8] = {0};
        uint32_t  index      = 0;

        if (sscanf(arg, "%7s %u", locName, &index) != 2) {
            backdoor_write_result("ERROR: expected 'SELECTADD <VA|FX> <index>'\n");
            return;
        }
        uint32_t  location   = (strncasecmp(locName, "FX", 2) == 0) ? (uint32_t)locationFx : (uint32_t)locationVa;
        tModule * module     = get_module_slot(gSlot, location, index);

        if ((module == NULL) || (module->type == 0)) {
            backdoor_write_result("ERROR: no module at that index\n");
            return;
        }

        if (!is_selected(module->key)) {
            selection_toggle(module->key);
        }
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "MOVESEL") == 0) {
        // MOVESEL <dColumn> <dRow> — moves the whole selection by a grid delta and then re-orders the
        // column exactly as dropping it would, through the same shift_selection_down() the release
        // calls. THE ONLY SCRIPTED ROUTE TO A GROUP DROP: a synthetic drag does not reach the app, so
        // without this the multi-module half of the shift can only be exercised by hand.
        //
        // Refuses with "no room" when the shift cannot place the group, and puts it back where it
        // was — the same answer canvas_module_drag_release() gives a drag it cannot land.
        int32_t        dCol        = 0;
        int32_t        dRow        = 0;

        if (sscanf(arg, "%d %d", &dCol, &dRow) != 2) {
            backdoor_write_result("ERROR: expected 'MOVESEL <dColumn> <dRow>'\n");
            return;
        }

        if (gSelection.count == 0) {
            backdoor_write_result("ERROR: nothing selected\n");
            return;
        }
        tUndoMoveEntry before[MAX_NUM_MODULES];
        uint32_t       beforeCount = module_positions_snapshot((uint32_t)gSlot, (uint32_t)gLocation, before);

        for (uint32_t si = 0; si < gSelection.count; si++) {
            tModule * member = get_module(gSelection.keys[si]);

            if (member == NULL) {
                continue;
            }
            int32_t   nc     = (int32_t)member->column + dCol;
            int32_t   nr     = (int32_t)member->row + dRow;

            member->column = (uint32_t)((nc < 0) ? 0 : ((nc > (int32_t)MAX_COLUMNS) ? (int32_t)MAX_COLUMNS : nc));
            member->row    = (uint32_t)((nr < 0) ? 0 : ((nr > (int32_t)MAX_ROWS) ? (int32_t)MAX_ROWS : nr));
        }

        bool           placed      = shift_selection_down();

        if (placed == false) {
            for (uint32_t i = 0; i < beforeCount; i++) {
                tModule * mod = get_module(before[i].key);

                if (mod != NULL) {
                    mod->column = before[i].oldColumn;
                    mod->row    = before[i].oldRow;
                }
            }
        }
        synthlib_request_redraw();
        backdoor_write_result(placed ? "OK\n" : "ERROR: no room\n");
    } else if (strcmp(cmd, "SELECT") == 0) {
        // SELECT <VA|FX> <index>, or SELECT NONE — the engine keys off the selection, and clicking a
        // module's header strip by coordinate was the single most error-prone step in driving it.
        char     locName[8] = {0};
        uint32_t index      = 0;

        if (strncasecmp(arg, "NONE", 4) == 0) {
            selection_clear();
            synthlib_request_redraw();
            backdoor_write_result("OK\n");
            return;
        }

        // SELECT with no argument reports what is selected, so a test can check the selection
        // rather than infer it from a screenshot. Without this the clear-on-switch behaviour is
        // invisible to anything driving the app from outside.
        if ((arg[0] == '\0') || (arg[0] == '?')) {
            char report[512] = {0};
            int  used        = snprintf(report, sizeof(report), "OK\ncount=%u\n", gSelection.count);

            for (uint32_t si = 0; (si < gSelection.count) && (used < (int)sizeof(report) - 48); si++) {
                used += snprintf(report + used, sizeof(report) - (size_t)used,
                                 "  slot=%u loc=%s index=%u\n",
                                 gSelection.keys[si].slot,
                                 (gSelection.keys[si].location == locationVa) ? "VA" : "FX",
                                 gSelection.keys[si].index);
            }

            backdoor_write_result(report);
            return;
        }

        if (sscanf(arg, "%7s %u", locName, &index) != 2) {
            backdoor_write_result("ERROR: expected 'SELECT <VA|FX> <index>' or 'SELECT NONE'\n");
            return;
        }
        {
            uint32_t  location = (strncasecmp(locName, "FX", 2) == 0) ? (uint32_t)locationFx : (uint32_t)locationVa;
            tModule * module   = get_module_slot(gSlot, location, index);

            if ((module == NULL) || (module->type == 0)) {
                backdoor_write_result("ERROR: no module at that index\n");
                return;
            }
            selection_set_single((tModuleKey){gSlot, location, index});
            synthlib_request_redraw();
            backdoor_write_result("OK\n");
        }
    } else if (strcmp(cmd, "SAVEFILE") == 0) {
        // SAVEFILE <path> — writes the current slot straight to a path, no save panel involved.
        // Driving the native panel with synthetic keystrokes is how a test patch got overwritten;
        // this exists so a round-trip can be checked without going anywhere near it.
        tMessageContent msg = {0};

        if ((arg == NULL) || (arg[0] == '\0')) {
            backdoor_write_result("ERROR: expected 'SAVEFILE <path>'\n");
            return;
        }
        msg.cmd                = eMsgCmdSavePatchFile;
        msg.patchFileData.slot = gSlot;
        strncpy(msg.patchFileData.filePath, arg, sizeof(msg.patchFileData.filePath) - 1);
        msg_send(&gToUsbThread, &msg);
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "NOTE") == 0) {
        // NOTE <midi note> plays, NOTE OFF releases. The last thing that needed a mouse to test the
        // sound engine end to end.
        int32_t note = 0;

        if (strncasecmp(arg, "OFF", 3) == 0) {
            sound_engine_note(-1, false);
            backdoor_write_result("OK\n");
            return;
        }

        if (sscanf(arg, "%d", &note) != 1) {
            backdoor_write_result("ERROR: expected 'NOTE <0-127>' or 'NOTE OFF'\n");
            return;
        }
        sound_engine_note(note, true);
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "SNDDUMP") == 0) {
        char text[8400] = {0};

        snprintf(text, sizeof(text), "OK\n%s", sound_engine_debug_text());
        backdoor_write_result(text);
    } else if (strcmp(cmd, "SNDSTATUS") == 0) {
        // Reads back what the Experimental menu would show, so a test can assert on why the engine
        // is or is not making a sound without taking a screenshot of a menu.
        char text[160] = {0};

        snprintf(text, sizeof(text), "OK\n%s\n", sound_engine_status_text());
        backdoor_write_result(text);
    } else if (strcmp(cmd, "SCROLL") == 0) {
        double xFraction = 0.0;
        double yFraction = 0.0;

        if (sscanf(arg, "%lf %lf", &xFraction, &yFraction) != 2) {
            backdoor_write_result("ERROR: expected 'SCROLL <x 0.0-1.0> <y 0.0-1.0>'\n");
            return;
        }
        // set_[xy]_scroll_bar() take a position along the scrollbar track in logical pixels;
        // clamp_scroll_bar() inside them pins anything past the end, so scaling the fraction by
        // the render size is enough to reach either extreme.
        set_x_scroll_bar(xFraction * (get_render_width() / gGlobalGuiScale));
        set_y_scroll_bar(yFraction * (get_render_height() / gGlobalGuiScale));
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "ZOOM") == 0) {
        double zoom = 0.0;

        if (sscanf(arg, "%lf", &zoom) != 1) {
            backdoor_write_result("ERROR: expected 'ZOOM <0.25-2.0>'\n");
            return;
        }
        set_zoom_factor(zoom, (tCoord){0.0, 0.0});
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "SPLIT") == 0) {
        // SPLIT VA | FX | BALANCE | <pixels>
        //
        // Where the Voice/FX divider sits. VA and FX slam it to an end, which is exactly what the
        // topbar's VA/FX buttons and the bar's own up/down arrows do; BALANCE is the double-arrow.
        // A number is the drag, as a Voice Area height in pixels, clamped the same way.
        //
        // This exists for RENDER CHECKS. A screenshot of a module face is framed by whatever the
        // divider leaves, and with the FX area taking half the window a four-row module does not
        // fit — so checking a face meant scrolling around it a screenshot at a time (CT: "You may
        // want to minimise the FX area via the dividing line when attempting to check the
        // renders"). A synthetic drag does not reach the app, which is the same reason SCROLL and
        // ZOOM are here.
        //
        // IT IS PATCH DATA, NOT A VIEW SETTING, and that matters for a measurement run. The divider
        // lives in gPatchDescr[slot].barPosition, so moving it marks the patch dirty and it travels
        // to the G2 and to file like any other edit - unlike SCROLL and ZOOM, which are purely
        // local. Frame with SPLIT before building the patch under test, not in the middle of one.
        char   what[16] = {0};
        double pixels   = 0.0;

        if (sscanf(arg, "%15s", what) != 1) {
            backdoor_write_result("ERROR: expected 'SPLIT <VA|FX|BALANCE|pixels>'\n");
            return;
        }

        if (strcasecmp(what, "VA") == 0) {
            split_view_show_full((uint32_t)locationVa);      // FX collapsed, Voice Area full height
        } else if (strcasecmp(what, "FX") == 0) {
            split_view_show_full((uint32_t)locationFx);
        } else if ((strcasecmp(what, "BALANCE") == 0) || (strcasecmp(what, "RESTORE") == 0)) {
            split_view_restore_balance();
        } else if (sscanf(arg, "%lf", &pixels) == 1) {
            split_view_set_position(pixels);
        } else {
            backdoor_write_result("ERROR: expected 'SPLIT <VA|FX|BALANCE|pixels>'\n");
            return;
        }
        synthlib_request_redraw();
        backdoor_write_result("OK\n");
    } else if (strcmp(cmd, "SCREENSHOT") == 0) {
        if (arg[0] == '\0') {
            backdoor_write_result("ERROR: expected 'SCREENSHOT <path>'\n");
            return;
        }
        backdoor_screenshot(arg);
    } else {
        char msg[128];

        snprintf(msg, sizeof(msg), "ERROR: unknown command '%s'\n", cmd);
        backdoor_write_result(msg);
    }
}

void backdoor_poll(void) {
    if (!backdoor_enabled()) {
        return;
    }
    const char * cmdPath   = backdoor_cmd_path();

    if (access(cmdPath, F_OK) != 0) {
        return;
    }
    FILE *       f         = fopen(cmdPath, "r");

    if (!f) {
        return;
    }
    char         line[512] = {0};

    if (!fgets(line, sizeof(line), f)) {
        line[0] = '\0';
    }
    fclose(f);
    remove(cmdPath);

    size_t       len       = strlen(line);

    while ((len > 0) && ((line[len - 1] == '\n') || (line[len - 1] == '\r'))) {
        line[--len] = '\0';
    }
    char         cmd[32]   = {0};
    char *       space     = strchr(line, ' ');

    if (space) {
        size_t cmdLen = (size_t)(space - line);

        if (cmdLen >= sizeof(cmd)) {
            cmdLen = sizeof(cmd) - 1;
        }
        memcpy(cmd, line, cmdLen);
        cmd[cmdLen] = '\0';
        backdoor_dispatch(cmd, space + 1);
    } else {
        strncpy(cmd, line, sizeof(cmd) - 1);
        backdoor_dispatch(cmd, "");
    }
}

#ifdef __cplusplus
}
#endif
