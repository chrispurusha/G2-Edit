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

#ifdef __cplusplus
extern "C" {
#endif

// System header files
#include <math.h>

// Disable warnings from external library headers etc.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Weverything"

#define GL_SILENCE_DEPRECATION    1
#include <GLFW/glfw3.h>

#pragma clang diagnostic pop

#include "dataBase.h"
#include "utilsGraphics.h"
#include "moduleResources.h"
#include "moduleGraphics.h"

tRectangle render_dial(tRectangle rectangle, uint32_t value) {  // Drop down into utilsGraphics?
    double angle  = 0.0;
    double radius = 0.0;
    double x      = 0;
    double y      = 0;

    radius = (rectangle.size.w / 2.0);
    x      = rectangle.coord.x + radius;
    y      = rectangle.coord.y + radius;
    angle  = value_to_angle(value);

    set_rbg_colour({0.5, 0.5, 0.5});
    render_circle_part_angle(moduleArea, {x, y}, radius, 0.0, 360.0, 25);
    set_rbg_colour({0.0, 0.0, 0.0});
    render_radial_line(moduleArea, {x, y}, radius, angle, 2.0);
    set_rbg_colour({0.0, 0.0, 0.0});
    return render_circle_line(moduleArea, {x, y}, radius, 25, 1.0);
}

void set_module_colour(uint32_t colour) {
    tRgb rgb = {0};

    rgb = gModuleColourMap[colour];
    set_rbg_colour(rgb);
}

// This might be too generic and won't be able to use, or we add extra params!
void render_param_common_dial(tCoord coord, uint32_t param, tModule * module, char * label, uint32_t range) {
    char buff[16] = {0};

    snprintf(buff, sizeof(buff), "%u", module->param[0][param].value);
    module->param[0][param].type  = paramTypeCommonDial;
    module->param[0][param].range = range;
    //module->param[0][param].rectangle = ;

    set_rbg_colour(RGB_BLACK);
    render_text(moduleArea, {{coord.x, coord.y - 15.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    render_text(moduleArea, {{coord.x, coord.y - 30.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, label);
    set_rbg_colour({0.2, 0.2, 0.2});
    module->param[0][param].rectangle = render_dial({coord, {FILTER_FREQ_RADIUS * 2.0, FILTER_FREQ_RADIUS * 2.0}}, module->param[0][param].value);
}

// This might become a common function for any modules with a bypass
void render_common_bypass(tCoord coord, uint32_t param, tModule * module) {
    module->param[0][param].type  = paramTypeBypass;
    module->param[0][param].range = 2;

    module->param[0][param].rectangle = draw_power_button(moduleArea, {coord, {BYPASS_BUTTON_WIDTH, BYPASS_BUTTON_HEIGHT}}, module->param[0][param].value != 0);
}

void render_common_keyboard_track(tCoord coord, uint32_t param, tModule * module) {
    uint32_t range     = ARRAY_SIZE(filterKbMap);
    char *   valString = (char *)filterKbMap[module->param[0][param].value];

    module->param[0][param].type  = paramTypeKeyboardTrack;
    module->param[0][param].range = range;
    set_rbg_colour(RGB_BLACK);
    render_text(moduleArea, {{coord.x, coord.y - 15.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Kbt");
    set_rbg_colour(RGB_BACKGROUND_GREY);
    module->param[0][param].rectangle = draw_toggle_button(moduleArea, {coord, {largest_text_width(range, (char **)filterKbMap, STANDARD_TEXT_HEIGHT), STANDARD_TEXT_HEIGHT}}, valString);
}

void render_FltClassic_db(tCoord coord, uint32_t param, tModule * module) {
    uint32_t range     = ARRAY_SIZE(filterDbMap);
    char *   valString = (char *)filterDbMap[module->param[0][param].value];

    module->param[0][param].type  = paramTypeFltClassicDb;
    module->param[0][param].range = ARRAY_SIZE(filterDbMap);
    set_rbg_colour(RGB_BACKGROUND_GREY);
    module->param[0][param].rectangle = draw_toggle_button(moduleArea, {coord, {largest_text_width(range, (char **)filterDbMap, STANDARD_TEXT_HEIGHT), STANDARD_TEXT_HEIGHT}}, valString);
}

void render_common_freq(tCoord coord, uint32_t param, tModule * module) {
    double freq     = 0.0;
    char   buff[16] = {0};

    freq = round(13.75 * pow(2, (double)module->param[0][param].value / 12.0) * 100.0) / 100.0;

    if (freq < 100) {
        snprintf(buff, sizeof(buff), "%.2fHz", freq);
    } else if (freq < 1000) {
        snprintf(buff, sizeof(buff), "%.1fHz", freq);
    } else if (freq < 10000) {
        snprintf(buff, sizeof(buff), "%.2fkHz", freq / 1000.0);
    } else {
        snprintf(buff, sizeof(buff), "%.1fkHz", freq / 1000.0);
    }
    module->param[0][param].type  = paramTypeFreq;
    module->param[0][param].range = 128;
    set_rbg_colour(RGB_BLACK);
    render_text(moduleArea, {{coord.x, coord.y - 15.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    render_text(moduleArea, {{coord.x, coord.y - 30.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Freq");
    set_rbg_colour({0.2, 0.2, 0.2});
    module->param[0][param].rectangle = render_dial({coord, FILTER_FREQ_RADIUS * 2.0, FILTER_FREQ_RADIUS * 2.0}, module->param[0][param].value);
}

void render_common_pitch(tCoord coord, uint32_t param, tModule * module) {
    double percent  = 0.0;
    double maxVal   = 200.0;
    char   buff[16] = {0};

    if (module->param[0][param].value < 127) {
        percent = round(((double)module->param[0][param].value * maxVal * 10.0) / 128.0) / 10.0;
    } else {
        percent = maxVal;     // Clip
    }
    module->param[0][param].type  = paramTypePitch;
    module->param[0][param].range = 128;
    snprintf(buff, sizeof(buff), "%.1f%%", percent);
    set_rbg_colour(RGB_BLACK);
    render_text(moduleArea, {{coord.x, coord.y - 15.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    render_text(moduleArea, {{coord.x, coord.y - 30.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Env");
    set_rbg_colour({0.2, 0.2, 0.2});
    module->param[0][param].rectangle = render_dial({coord, FILTER_FREQ_RADIUS * 2.0, FILTER_FREQ_RADIUS * 2.0}, module->param[0][param].value);
}

void render_common_resonance(tCoord coord, uint32_t param, tModule * module) {
    double res      = 0.0;
    double maxVal   = 100.0;
    char   buff[16] = {0};

    if (module->param[0][param].value < 127) {
        res = round(((double)module->param[0][param].value * maxVal * 10.0) / 128.0) / 10.0;
    } else {
        res = maxVal;     // Clip
    }
    module->param[0][param].type  = paramTypeResonance;
    module->param[0][param].range = 128;
    snprintf(buff, sizeof(buff), "%.1f", res);
    set_rbg_colour(RGB_BLACK);
    render_text(moduleArea, {{coord.x, coord.y - 15.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    render_text(moduleArea, {{coord.x, coord.y - 30.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Res");
    set_rbg_colour({0.2, 0.2, 0.2});
    module->param[0][param].rectangle = render_dial({coord, FILTER_FREQ_RADIUS * 2.0, FILTER_FREQ_RADIUS * 2.0}, module->param[0][param].value);
}

void render_common_gc(tCoord coord, uint32_t param, tModule * module) {
    char * valString = "GC";

    module->param[0][param].type  = paramTypeGainControl;
    module->param[0][param].range = 2;

    if (module->param[0][param].value != 0) {
        set_rbg_colour({0.3, 0.7, 0.3});         // Green when ON
    } else {
        set_rbg_colour(RGB_BACKGROUND_GREY);     // Grey when OFF
    }
    module->param[0][param].rectangle = draw_toggle_button(moduleArea, {coord, get_text_width(valString, STANDARD_TEXT_HEIGHT), STANDARD_TEXT_HEIGHT}, valString);
}

void render_FltMulti_db(tCoord coord, uint32_t param, tModule * module) {
    uint32_t range     = ARRAY_SIZE(fltMultiDbMap);
    char *   valString = (char *)fltMultiDbMap[module->param[0][param].value];

    module->param[0][param].type  = paramTypeFltMultiDb;
    module->param[0][param].range = range;
    set_rbg_colour(RGB_BACKGROUND_GREY);
    module->param[0][param].rectangle = draw_toggle_button(moduleArea, {coord, largest_text_width(range, (char **)fltMultiDbMap, STANDARD_TEXT_HEIGHT), STANDARD_TEXT_HEIGHT}, valString);
}

void render_connector(tModule * module, uint32_t connectorIndex, tConnectorDir dir, tConnectorType type, tCoord coord) {
    module->connector[connectorIndex].coord = coord;  // Register where we're rendering this connector, for cable connecting
    module->connector[connectorIndex].dir   = dir;    // Ultimately, should be constant in the structures, we shouldn't have to do this here
    module->connector[connectorIndex].type  = type;

    set_rbg_colour(connectorColourMap[module->connector[connectorIndex].dir][module->connector[connectorIndex].type]);
    module->connector[connectorIndex].rectangle = render_circle_part(moduleArea, coord, 8.0, 10.0, 0.0, 10.0);  // Should be zoomfactored
    set_rbg_colour(RGB_BLACK);
    render_circle_part(moduleArea, coord, 4.0, 10.0, 0.0, 10.0);
}

void render_param_EnvAdsr_attack(tCoord coord, uint32_t param, tModule * module) {
    render_param_common_dial(coord, param, module, "Attack", 128);
}

void render_param_EnvAdsr_delay(tCoord coord, uint32_t param, tModule * module) {
    render_param_common_dial(coord, param, module, "Delay", 128);
}

void render_param_EnvAdsr_sustain(tCoord coord, uint32_t param, tModule * module) {
    render_param_common_dial(coord, param, module, "Sus", 128);
}

void render_param_EnvAdsr_release(tCoord coord, uint32_t param, tModule * module) {
    render_param_common_dial(coord, param, module, "Rel", 128);
}

void render_module_common(tRectangle rectangle, tModule * module) {
    if (module == NULL) {
        return;
    }
    uint32_t    param      = 0;
    uint32_t    connector  = 0;
    tModuleType moduleType = module->type;

    for (size_t i = 0; i < ARRAY_SIZE(paramLocationList); i++) {
        if (paramLocationList[i].moduleType == moduleType) {
            paramLocationList[i].renderFunction(
                (tCoord){rectangle.coord.x + paramLocationList[i].offsetX, rectangle.coord.y + paramLocationList[i].offsetY},
                param++, module);
        }
    }

    for (size_t i = 0; i < ARRAY_SIZE(connectorLocationList); i++) {
        if (connectorLocationList[i].moduleType == moduleType) {
            connectorLocationList[i].renderFunction(module, connector++,
                                                    connectorLocationList[i].direction,
                                                    connectorLocationList[i].type,
                                                    (tCoord){rectangle.coord.x + connectorLocationList[i].offsetX, rectangle.coord.y + connectorLocationList[i].offsetY});
        }
    }
}

void render_module(tModule * module) {
    double     moduleHeight               = gModuleProperties[module->type].height;
    double     xPos                       = module->column * MODULE_X_SPAN;
    double     yPos                       = module->row * MODULE_Y_SPAN;
    double     xWidth                     = MODULE_WIDTH;
    double     yHeight                    = (moduleHeight * MODULE_Y_SPAN) - MODULE_Y_GAP;
    char       buff[MODULE_NAME_SIZE + 1] = {0};

    tRectangle moduleRectangle = {{xPos, yPos}, {xWidth, yHeight}};

    set_module_colour(module->colour);
    module->rectangle = render_rectangle_with_border(moduleArea, moduleRectangle); // Add zoom factor for border - really needs to scale the whole thing!
    render_module_common(moduleRectangle, module);
    write_module(module->key, module);                                             // Save calculated coords

    snprintf(buff, sizeof(buff), "%s", module->name);
    set_rbga_colour(RGBA_BLACK_ON_TRANSPARENT);
    render_text(moduleArea, {{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    
    // Temporary items purely for development debug
    snprintf(buff, sizeof(buff), "(%s)", gModuleProperties[module->type].name);

    render_text(moduleArea, {{moduleRectangle.coord.x + 120.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    snprintf(buff, sizeof(buff), "%u", module->key.index);
    render_text(moduleArea, {{moduleRectangle.coord.x + moduleRectangle.size.w - 30.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

    if (module->upRate) {
        render_text(moduleArea, {{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 15.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Uprated");
    }

    if (module->isLed) {
        render_text(moduleArea, {{moduleRectangle.coord.x + 60.0, moduleRectangle.coord.y + 15.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, "Led");
    }

    if (module->modeCount > 0) {
        snprintf(buff, sizeof(buff), "Modes %u", module->modeCount);
        render_text(moduleArea, {{moduleRectangle.coord.x + 150.0, moduleRectangle.coord.y + 15.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    }
}

void render_modules(void) {
    tModule    module      = {0};
    bool       validModule = false;
    tRectangle area        = module_area();

    reset_walk_module();

    do {
        validModule = walk_next_module(&module);

        if (validModule && module.key.location == 1 && module.type != moduleTypeUnknown0) {
            render_module(&module);
        }
    } while (validModule);

    // Draw background areas
    set_rbg_colour(RGB_BACKGROUND_GREY);
    render_rectangle(mainArea, {{0.0, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    render_rectangle(mainArea, {{0.0, area.coord.y - MODULE_MARGIN}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
    render_rectangle(mainArea, {{area.coord.x + area.size.w, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    render_rectangle(mainArea, {{0.0, area.coord.y + area.size.h}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
}

void render_cable_from_to(tConnector from, tConnector to) {
    tCoord control = {0};

    if (from.coord.x == to.coord.x) {
        control.x = from.coord.x;
        control.y = fmin(from.coord.y, to.coord.y);
    } else {
        control.x = ((from.coord.x + to.coord.x) / 2.0);
        control.y = fmax(from.coord.y, to.coord.y) + 40.0;
    }
    render_bezier_curve(moduleArea, from.coord, control, to.coord, 4.0, 15);
}

void render_cable(tCable * cable) {
    tModule moduleFrom = {0};
    tModule moduleTo   = {0};

    if (read_module({cable->key.location, cable->key.moduleFromIndex}, &moduleFrom) == false) {
        return;
    }

    if (read_module({cable->key.location, cable->key.moduleToIndex}, &moduleTo) == false) {
        return;
    }
    set_rbg_colour(cableColourMap[cable->colour]);

    int fromConnectorIndex = find_index_from_io_count(&moduleFrom, (tConnectorDir)cable->key.linkType, cable->key.connectorFromIoCount);

    int toConnectorIndex = find_index_from_io_count(&moduleTo, connectorDirIn, cable->key.connectorToIoCount);

    if (fromConnectorIndex != -1 && toConnectorIndex != -1) {
        render_cable_from_to(moduleFrom.connector[fromConnectorIndex], moduleTo.connector[toConnectorIndex]);
    }
}

void render_cables(void) {
    tCable cable      = {0};
    bool   validCable = false;

    reset_walk_cable();

    do {
        validCable = walk_next_cable(&cable);

        if (validCable && cable.key.location == 1) {
            render_cable(&cable);
        }
    } while (validCable);
}

#ifdef __cplusplus
}
#endif
