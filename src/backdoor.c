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
// Notes: Docs/code-notes/backdoor.c.md - "// notes §k" refers there.

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
#include "moduleReplace.h"
#include "palette.h"
#include "selection.h"
#include "soundEngine.h"
#include "cableChain.h"
#include "appMenuBar.h"
#include "contextMenu.h"
#include "utilsGraphics.h"
#include "graphics.h"
#include "splitView.h"
#include "backdoor.h"

// notes §1
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

    // notes §2
    used += (size_t)snprintf(out + used, outMax - used, "OK\nslot=%u voices=%u monoPoly=%u\n",
                             (unsigned)gSlot, (unsigned)gPatchDescr[gSlot].voiceCount,
                             (unsigned)gPatchDescr[gSlot].monoPoly);

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

// notes §3
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

            // eng=: the engine's meter for the same module, or "-" (sound engine reference §1).
            if (volCount > 0) {
                fprintf(file, " eng=");

                for (uint32_t i = 0; i < volCount; i++) {
                    uint32_t engine = 0;

                    if ((i < 2) && sound_engine_module_meter(locs[l], index, i, &engine)) {
                        fprintf(file, "%s%u", (i == 0) ? "" : ",", (unsigned)engine);
                    } else {
                        fprintf(file, "%s-", (i == 0) ? "" : ",");
                    }
                }
            }
            fprintf(file, "\n");
        }
    }

    fclose(file);
}

// notes §4
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

// notes §5
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

// notes §6
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

// notes §7
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
        // notes §8
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
    } else if (strcmp(cmd, "PALETTE") == 0) {
        // notes §9
        char sub[32]  = {0};
        char rest[64] = {0};
        int  given    = sscanf(arg, "%31s %63s", sub, rest);

        if (given < 1) {
            backdoor_write_result("ERROR: expected 'PALETTE ON|OFF|TOGGLE|STATUS|GROUP <name>|ADD <module>'\n");
            return;
        }

        if (strcasecmp(sub, "ON") == 0) {
            palette_set_open(true);
        } else if (strcasecmp(sub, "OFF") == 0) {
            palette_set_open(false);
        } else if (strcasecmp(sub, "TOGGLE") == 0) {
            palette_toggle();
        } else if (strcasecmp(sub, "GROUP") == 0) {
            uint32_t g     = 0;
            bool     found = false;

            for (g = 0; g < (uint32_t)palGroupCount; g++) {
                if (strcasecmp(palette_group_name((tPaletteGroup)g), rest) == 0) {
                    palette_select_group((tPaletteGroup)g);
                    found = true;
                    break;
                }
            }

            if (!found) {
                backdoor_write_result("ERROR: no such palette group\n");
                return;
            }
        } else if (strcasecmp(sub, "ADD") == 0) {
            tModuleType found = (tModuleType)0;

            for (uint32_t t = 1; t < (uint32_t)moduleTypeMax; t++) {
                if (strcmp(gModuleProperties[t].name, rest) == 0) {
                    found = (tModuleType)t;
                    break;
                }
            }

            if (found == (tModuleType)0) {
                backdoor_write_result("ERROR: no module of that name\n");
                return;
            }

            if (palette_add_module(found) == false) {
                backdoor_write_result("ERROR: could not add (location full?)\n");
                return;
            }
        } else if (strcasecmp(sub, "DRAG") == 0) {
            // PALETTE DRAG <module> <x> <y> — put the palette into a drag at that point, so the
            // ghost can be screenshotted. A synthetic mouse drag never reaches the canvas, so this
            // is the only way to see the drag rendering without a person at the machine.
            char        name[64] = {0};
            double      x        = 0.0;
            double      y        = 0.0;
            tModuleType found    = (tModuleType)0;

            if (sscanf(arg, "%*s %63s %lf %lf", name, &x, &y) != 3) {
                backdoor_write_result("ERROR: expected 'PALETTE DRAG <module> <x> <y>'\n");
                return;
            }

            for (uint32_t t = 1; t < (uint32_t)moduleTypeMax; t++) {
                if (strcmp(gModuleProperties[t].name, name) == 0) {
                    found = (tModuleType)t;
                    break;
                }
            }

            if (found == (tModuleType)0) {
                backdoor_write_result("ERROR: no module of that name\n");
                return;
            }
            palette_begin_drag(found, (tCoord){x, y});
        } else if (strcasecmp(sub, "DROP") == 0) {
            // PALETTE DROP <x> <y> — release the drag started by PALETTE DRAG.
            double x = 0.0;
            double y = 0.0;

            if (sscanf(arg, "%*s %lf %lf", &x, &y) != 2) {
                backdoor_write_result("ERROR: expected 'PALETTE DROP <x> <y>'\n");
                return;
            }
            palette_cursor_moved((tCoord){x, y});
            (void)palette_left_up((tCoord){x, y});
        } else if (strcasecmp(sub, "STATUS") != 0) {
            backdoor_write_result("ERROR: expected ON|OFF|TOGGLE|STATUS|GROUP|ADD\n");
            return;
        }
        {
            char     msg[256] = {0};
            uint32_t count    = 0;

            {
                tModuleType tiles[32];
                count = palette_group_modules(palette_selected_group(), tiles, 32);
            }
            snprintf(msg, sizeof(msg), "OK open=%s group=%s tiles=%u bandHeight=%.0f newColour=%u\n",
                     palette_is_open() ? "yes" : "no",
                     palette_group_name(palette_selected_group()), count, palette_band_height(),
                     palette_new_module_colour());
            backdoor_write_result(msg);
        }
    } else if (strcmp(cmd, "REPLACE") == 0) {
        // notes §10
        char         name[64] = {0};
        uint32_t     index    = 0;
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

        if (sscanf(rest, "%u %63s", &index, name) != 2) {
            backdoor_write_result("ERROR: expected 'REPLACE [VA|FX] <index> <name>'\n");
            return;
        }
        tModuleType  found    = (tModuleType)0;

        for (uint32_t t = 1; t < (uint32_t)moduleTypeMax; t++) {
            if (strcmp(gModuleProperties[t].name, name) == 0) {
                found = (tModuleType)t;
                break;
            }
        }

        if ((found == (tModuleType)0) && (strcasecmp(name, "LIST") != 0)) {
            char msg[128];

            snprintf(msg, sizeof(msg), "ERROR: no module named '%s'\n", name);
            backdoor_write_result(msg);
            return;
        }
        gLocation = area;
        tModuleKey   key      = {gSlot, area, index};

        // REPLACE ... LIST reports what the module right-click menu WOULD offer rather than doing
        // anything - the same module_replace_candidates() call the menu makes - so a greyed-out
        // "Replace with" can be diagnosed without a mouse.
        if (strcasecmp(name, "LIST") == 0) {
            tModule *   module   = get_module(key);
            tModuleType cand[32] = {0};
            uint32_t    count    = 0;
            char        msg[512] = {0};
            size_t      used     = 0;

            if (module == NULL) {
                backdoor_write_result("ERROR: no module at that loc/index\n");
                return;
            }
            count = module_replace_candidates(module->type, cand, 32);
            used  = (size_t)snprintf(msg, sizeof(msg), "OK %s group=%d candidates=%u:",
                                     gModuleProperties[module->type].name,
                                     (int)module_group(module->type), count);

            for (uint32_t c = 0; (c < count) && (used < (sizeof(msg) - 32)); c++) {
                used += (size_t)snprintf(&msg[used], sizeof(msg) - used, " %s",
                                         gModuleProperties[cand[c]].name);
            }

            snprintf(&msg[used], sizeof(msg) - used, "\n");
            backdoor_write_result(msg);
            return;
        }
        bool ok = module_replace(key, found);

        synthlib_request_redraw();
        backdoor_write_result(ok ? "OK\n" : "ERROR: not replaceable (no group, no role table, or column full)\n");
    } else if ((strcmp(cmd, "ADDMODULE") == 0) || (strcmp(cmd, "DEVADDMODULE") == 0)) {
        // notes §11
        bool         toDevice = true;

        // notes §12
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

        // notes §13
        if (given < 3) {
            row = backdoor_next_free_row(gSlot, gLocation, col);
        }
        // notes §14
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
        // notes §15
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
        // notes §16
        cable_chain_recolour(key.slot, key.location,
                             (tCableNode){key.moduleToIndex, key.connectorToIoCount, false});

        // notes §17
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
        // notes §18
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
        // notes §19
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
        // notes §20
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
        // notes §21
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
    } else if (strcmp(cmd, "VOICES") == 0) {
        // notes §22
        char     mode[16] = {0};
        uint32_t voices   = 0;
        int      got      = sscanf(arg, "%u %15s", &voices, mode);

        if ((got < 1) || (voices < 1) || (voices > 32)) {
            backdoor_write_result("ERROR: expected 'VOICES <1-32> [poly|mono|legato]'\n");
            return;
        }
        gPatchDescr[gSlot].voiceCount = (uint8_t)voices;

        if (got == 2) {
            if (strcasecmp(mode, "poly") == 0) {
                gPatchDescr[gSlot].monoPoly = (uint8_t)monoPolyPoly;
            } else if (strcasecmp(mode, "mono") == 0) {
                gPatchDescr[gSlot].monoPoly = (uint8_t)monoPolyMono;
            } else if (strcasecmp(mode, "legato") == 0) {
                gPatchDescr[gSlot].monoPoly = (uint8_t)monoPolyLegato;
            } else {
                backdoor_write_result("ERROR: mode must be poly, mono or legato\n");
                return;
            }
        }
        char     text[96];

        snprintf(text, sizeof(text), "OK\nvoices=%u monoPoly=%u (PUSH to send)\n",
                 (unsigned)gPatchDescr[gSlot].voiceCount, (unsigned)gPatchDescr[gSlot].monoPoly);
        backdoor_write_result(text);
    } else if (strcmp(cmd, "DEVNOTES") == 0) {
        // notes §23
        uint32_t        slot   = gSlot;
        uint32_t        before = atomic_load(&gNote2Updates);
        uint32_t        waited = 0;
        tMessageContent msg    = {0};
        char            text[1024];
        int             used   = 0;

        (void)sscanf(arg, "%u", &slot);

        if (slot >= MAX_SLOTS) {
            backdoor_write_result("ERROR: slot must be 0-3\n");
            return;
        }
        msg.cmd  = eMsgCmdGetCurrentNote;
        msg.slot = slot;
        msg_send(&gToUsbThread, &msg);

        while ((atomic_load(&gNote2Updates) == before) && (waited < 1000)) {
            usleep(10000);
            waited += 10;
        }

        if (atomic_load(&gNote2Updates) == before) {
            backdoor_write_result("ERROR: no current-note reply within 1s (is the G2 online?)\n");
            return;
        }
        used     = snprintf(text, sizeof(text), "OK\nslot=%u bytes=%u\nraw=", slot, gNote2Size[slot]);

        for (uint32_t i = 0; (i < gNote2Size[slot]) && (used < (int)sizeof(text) - 4); i++) {
            used += snprintf(&text[used], sizeof(text) - (size_t)used, "%02x", gNote2[slot][i]);
        }

        snprintf(&text[used], sizeof(text) - (size_t)used, "\n");
        backdoor_write_result(text);
    } else if (strcmp(cmd, "COMMS") == 0) {
        // notes §24
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
        // notes §25
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
        // notes §26
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
        // notes §27
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
