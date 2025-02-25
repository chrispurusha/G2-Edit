/*
 * The G2 Editor application.
 *
 * Copyright (C) 2024 Chris Turner <chris_purusha@icloud.com>
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

#include <ft2build.h>
#include FT_FREETYPE_H
#pragma clang diagnostic pop

#include "dataBase.h"
#include "utilsGraphics.h"
#include "moduleResources.h"
#include "moduleGraphics.h"

extern int           gRenderWidth;
extern int           gRenderHeight;
extern tScrollState  gScrollState;
extern double        gZoomFactor;


tRectangle module_area(void) {
    double left   = MODULE_MARGIN;
    double top    = TOP_BAR_HEIGHT + MODULE_MARGIN;
    double width  = gRenderWidth - SCROLLBAR_WIDTH - (MODULE_MARGIN * 2.0);
    double height = gRenderHeight - TOP_BAR_HEIGHT - SCROLLBAR_WIDTH - (MODULE_MARGIN * 2.0);

    return (tRectangle){{left, top}, {width, height}};
}

void render_dial(tRectangle rectangle, uint32_t value) {
    double angle  = 0.0;
    double radius = 0.0;
    double x      = 0;
    double y      = 0;

    radius = (rectangle.size.w / 2.0);
    x      = rectangle.coord.x + radius;
    y      = rectangle.coord.y + radius;
    angle  = value_to_angle(value);

    set_rbg_colour({0.0, 0.5, 0.0});
    render_circle_part_angle({x, y}, radius, 0.0, 360.0, 25);
    set_rbg_colour({0.0, 0.0, 0.0});
    render_radial_line({x, y}, radius, angle, 2.0);
    set_rbg_colour({0.0, 0.0, 0.0});
    render_circle_line({x, y}, radius, 25, 1.0);
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
    module->param[0][param].type = paramTypeDial;
    module->param[0][param].range = range;
    module->param[0][param].rectangle = {{coord.x, coord.y}, {FILTER_FREQ_RADIUS * 2.0 * gZoomFactor, FILTER_FREQ_RADIUS * 2.0 * gZoomFactor}};

    set_rbg_colour(RGB_BLACK);
    render_text({{coord.x, coord.y - (15.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, buff);
    render_text({{coord.x, coord.y - (30.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, label);
    set_rbg_colour({0.2, 0.2, 0.2});
    render_dial(module->param[0][param].rectangle, module->param[0][param].value);
}

// This might become a common function for any modules with a bypes
void render_FltClassic_bypass(tCoord coord, uint32_t param, tModule * module) {
    module->param[0][param].type      = paramTypeToggle;
    module->param[0][param].range     = 2;
    module->param[0][param].rectangle = {{coord.x, coord.y}, {BYPASS_BUTTON_WIDTH * gZoomFactor, BYPASS_BUTTON_HEIGHT * gZoomFactor}};

    draw_power_button(module->param[0][param].rectangle, module->param[0][param].value != 0);
}

void render_FltClassic_keyboard_track(tCoord coord, uint32_t param, tModule * module) {
    char buff[16] = {0};

    module->param[0][param].type      = paramTypeToggle;
    module->param[0][param].range     = 5;
    module->param[0][param].rectangle = {{coord.x, coord.y}, {BYPASS_BUTTON_WIDTH * gZoomFactor, BYPASS_BUTTON_HEIGHT * gZoomFactor}};
    snprintf(buff, sizeof(buff), "%s", filterKbMap[module->param[0][param].value]);
    set_rbg_colour(RGB_BLACK);
    render_text({{coord.x, coord.y - (15.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, buff);
    render_text({{coord.x, coord.y - (30.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, "Kb");
    set_rbg_colour({0.0, 0.0, 0.7});
    render_rectangle(module->param[0][param].rectangle);
}

void render_FltClassic_db(tCoord coord, uint32_t param, tModule * module) {
    char buff[16] = {0};

    module->param[0][param].type      = paramTypeToggle;
    module->param[0][param].range     = 3;
    module->param[0][param].rectangle = {{coord.x, coord.y}, {BYPASS_BUTTON_WIDTH * gZoomFactor, BYPASS_BUTTON_HEIGHT * gZoomFactor}};
    snprintf(buff, sizeof(buff), "%s", filterDbMap[module->param[0][param].value]);
    set_rbg_colour(RGB_BLACK);
    render_text({{coord.x, coord.y - (15.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, buff);
    render_text({{coord.x, coord.y - (30.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, "dB");
    set_rbg_colour({0.0, 0.0, 0.7});
    render_rectangle(module->param[0][param].rectangle);
}

void render_FltClassic_freq(tCoord coord, uint32_t param, tModule * module) {
    double freq     = 0.0;
    char   buff[16] = {0};

    freq = round(13.75 * pow(2, (double)module->param[0][param].value / 12.0) * 100.0) / 100.0;

    if (freq < 100) {
        snprintf(buff, sizeof(buff), "%.2fHz", freq);
    }
    else if (freq < 1000) {
        snprintf(buff, sizeof(buff), "%.1fHz", freq);
    }
    else if (freq < 10000) {
        snprintf(buff, sizeof(buff), "%.2fkHz", freq / 1000.0);
    }
    else {
        snprintf(buff, sizeof(buff), "%.1fkHz", freq / 1000.0);
    }

    module->param[0][param].type      = paramTypeDial;
    module->param[0][param].range     = 128;
    module->param[0][param].rectangle = {{coord.x, coord.y}, {FILTER_FREQ_RADIUS * 2.0 * gZoomFactor, FILTER_FREQ_RADIUS * 2.0 * gZoomFactor}};
    set_rbg_colour(RGB_BLACK);
    render_text({{coord.x, coord.y - (15.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, buff);
    render_text({{coord.x, coord.y - (30.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, "Freq");
    set_rbg_colour({0.2, 0.2, 0.2});
    render_dial(module->param[0][param].rectangle, module->param[0][param].value);
}

void render_FltClassic_pitch(tCoord coord, uint32_t param, tModule * module) {
    double percent  = 0.0;
    double maxVal   = 200.0;
    char   buff[16] = {0};

    if (module->param[0][param].value < 127) {
        percent = round(((double)module->param[0][param].value * maxVal * 10.0) / 128.0) / 10.0;
    }
    else {
        percent = maxVal;     // Clip
    }
    module->param[0][param].type      = paramTypeDial;
    module->param[0][param].range     = 128;
    module->param[0][param].rectangle = {{coord.x, coord.y}, {FILTER_FREQ_RADIUS * 2.0 * gZoomFactor, FILTER_FREQ_RADIUS * 2.0 * gZoomFactor}};
    snprintf(buff, sizeof(buff), "%.1f%%", percent);
    set_rbg_colour(RGB_BLACK);
    render_text({{coord.x, coord.y - (15.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, buff);
    render_text({{coord.x, coord.y - (30.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, "Env");
    set_rbg_colour({0.2, 0.2, 0.2});
    render_dial(module->param[0][param].rectangle, module->param[0][param].value);
}

void render_FltClassic_resonance(tCoord coord, uint32_t param, tModule * module) {
    double res      = 0.0;
    double maxVal   = 100.0;
    char   buff[16] = {0};

    if (module->param[0][param].value < 127) {
        res = round(((double)module->param[0][param].value * maxVal * 10.0) / 128.0) / 10.0;
    }
    else {
        res = maxVal;     // Clip
    }
    module->param[0][param].type      = paramTypeDial;
    module->param[0][param].range     = 128;
    module->param[0][param].rectangle = {{coord.x, coord.y}, {FILTER_FREQ_RADIUS * 2.0 * gZoomFactor, FILTER_FREQ_RADIUS * 2.0 * gZoomFactor}};
    snprintf(buff, sizeof(buff), "%.1f", res);
    set_rbg_colour(RGB_BLACK);
    render_text({{coord.x, coord.y - (15.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, buff);
    render_text({{coord.x, coord.y - (30.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, "Res");
    set_rbg_colour({0.2, 0.2, 0.2});
    render_dial(module->param[0][param].rectangle, module->param[0][param].value);
}

void render_connector(tCoord coord, uint32_t connector, tConnectorType connectorType, tModule * module) {
    module->connector[connector][connectorType] = coord;  // Register where we're rendering this connector, for cable connecting
    
    set_rbg_colour(connectorColourMap[connectorType]);
    render_circle_part(coord, 6.0 * gZoomFactor, 10.0, 0.0, 10.0);
    set_rbg_colour(RGB_BLACK);
    render_circle_part(coord, 3.0 * gZoomFactor, 10.0, 0.0, 10.0);
    
    module->numConnectors++; // Ultimately, we might want to pre-calculate this per module
}

void render_FltClassic(tRectangle rectangle, tModule * module) {
    uint32_t param = 0;

    render_FltClassic_freq({rectangle.coord.x + (105.0 + FILTER_FREQ_RADIUS) * gZoomFactor, (rectangle.coord.y + 80.0 * gZoomFactor)}, param++, module);
    render_FltClassic_pitch({rectangle.coord.x + (15.0 + FILTER_FREQ_RADIUS) * gZoomFactor, rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);
    render_FltClassic_keyboard_track({rectangle.coord.x + (75.0 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);
    render_FltClassic_resonance({rectangle.coord.x + ((160.0 + FILTER_FREQ_RADIUS) * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);
    render_FltClassic_db({rectangle.coord.x + (210.0 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);
    render_FltClassic_bypass({rectangle.coord.x + (250 * gZoomFactor), rectangle.coord.y + (50.0 * gZoomFactor)}, param++, module);

    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + (20.0 * gZoomFactor)}, 0, connectorTypeAudioIn, module);
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + rectangle.size.h - (20.0 * gZoomFactor)}, 0, connectorTypeAudioOut, module);
    render_connector({rectangle.coord.x + (15.0 * gZoomFactor), rectangle.coord.y + rectangle.size.h - (20.0 * gZoomFactor)}, 1, connectorTypeControlIn, module);
    render_connector({rectangle.coord.x + (15.0 * gZoomFactor), rectangle.coord.y + (50.0 * gZoomFactor)}, 0, connectorTypeControlIn, module);
}

// fltMulti -- builds on fltClassic components

void render_FltMulti_db(tCoord coord, uint32_t param, tModule * module) {
    char buff[16] = {0};

    module->param[0][param].type      = paramTypeToggle;
    module->param[0][param].range     = 2;
    module->param[0][param].rectangle = {{coord.x, coord.y}, {BYPASS_BUTTON_WIDTH, BYPASS_BUTTON_HEIGHT}};
    snprintf(buff, sizeof(buff), "%s", fltMultiDbMap[module->param[0][param].value]);
    set_rbg_colour(RGB_BLACK);
    render_text({{coord.x, coord.y - 15.0}, {10.0, 10.0}}, buff);
    render_text({{coord.x, coord.y - 30.0}, {10.0, 10.0}}, "dB/Oct");
    set_rbg_colour({0.0, 0.0, 0.7});
    render_rectangle(module->param[0][param].rectangle);
}

// Gain control
void render_FltMulti_GV(tCoord coord, uint32_t param, tModule * module) {
    module->param[0][param].type      = paramTypeToggle;
    module->param[0][param].range     = 2;
    module->param[0][param].rectangle = {{coord.x, coord.y}, {BYPASS_BUTTON_WIDTH, BYPASS_BUTTON_HEIGHT}};
    draw_text_button(module->param[0][param].rectangle, "GC", module->param[0][param].value != 0);
}

void render_FltMulti(tRectangle rectangle, tModule * module) {
    uint32_t param = 0;

    render_FltClassic_freq({rectangle.coord.x + (105.0 + FILTER_FREQ_RADIUS) * gZoomFactor, rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);
    render_FltClassic_pitch({rectangle.coord.x + (15.0 + FILTER_FREQ_RADIUS) * gZoomFactor, rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);
    render_FltClassic_keyboard_track({rectangle.coord.x + (75.0 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);
    render_FltClassic_resonance({rectangle.coord.x + (160.0 + FILTER_FREQ_RADIUS) * gZoomFactor, rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);
    render_FltMulti_db({rectangle.coord.x + (210.0 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);
    render_FltClassic_bypass({rectangle.coord.x + (230 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, param++, module);

    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + (20.0 * gZoomFactor)}, 0, connectorTypeAudioIn, module);
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + (40.0 * gZoomFactor)}, 0, connectorTypeAudioOut, module);
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + (60.0 * gZoomFactor)}, 0, connectorTypeAudioOut, module);
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + rectangle.size.h - (20.0 * gZoomFactor)}, 0, connectorTypeAudioOut, module);
    render_connector({rectangle.coord.x + (15.0 * gZoomFactor), rectangle.coord.y + rectangle.size.h - (20.0 * gZoomFactor)}, 1, connectorTypeControlIn, module);
    render_connector({rectangle.coord.x + (15.0 * gZoomFactor), rectangle.coord.y + (50.0 * gZoomFactor)}, 0, connectorTypeControlIn, module);
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

void render_EnvAdsr(tRectangle rectangle, tModule * module) {
    render_param_EnvAdsr_attack({rectangle.coord.x + (20.0 + FILTER_FREQ_RADIUS) * gZoomFactor, rectangle.coord.y + (80.0 * gZoomFactor)}, 1, module);
    render_param_EnvAdsr_delay({rectangle.coord.x + (60.0 + FILTER_FREQ_RADIUS) * gZoomFactor, rectangle.coord.y + (80.0 * gZoomFactor)}, 2, module);
    render_param_EnvAdsr_sustain({rectangle.coord.x + (100.0 + FILTER_FREQ_RADIUS) * gZoomFactor, rectangle.coord.y + (80.0 * gZoomFactor)}, 3, module);
    render_param_EnvAdsr_release({rectangle.coord.x + (140.0 + FILTER_FREQ_RADIUS) * gZoomFactor, rectangle.coord.y + (80.0 * gZoomFactor)}, 4, module);

    render_connector({rectangle.coord.x + rectangle.size.w - (30.0 * gZoomFactor), rectangle.coord.y + rectangle.size.h - (20.0 * gZoomFactor)}, 0, connectorTypeControlOut, module);
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + rectangle.size.h - (20.0 * gZoomFactor)}, 1, connectorTypeAudioOut, module);   // Don't know if 1 is correct
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + (20.0 * gZoomFactor)}, 0, connectorTypeAudioIn, module);
}

void render_Mix4to1c(tRectangle rectangle, tModule * module) {
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + rectangle.size.h - (20.0 * gZoomFactor)}, 0, connectorTypeAudioOut, module);
    render_connector({rectangle.coord.x + (60 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, 0, connectorTypeAudioIn, module);
    render_connector({rectangle.coord.x + (80 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, 1, connectorTypeAudioIn, module);
    render_connector({rectangle.coord.x + (100 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, 2, connectorTypeAudioIn, module);
    render_connector({rectangle.coord.x + (120 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, 3, connectorTypeAudioIn, module);
}

void render_OscShpB(tRectangle rectangle, tModule * module) {
    render_connector({rectangle.coord.x + (150.0 * gZoomFactor), rectangle.coord.y + (80.0 * gZoomFactor)}, 0, connectorTypeAudioOut, module);
}

void render_StChorus(tRectangle rectangle, tModule * module) {
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + (20.0 * gZoomFactor)}, 0, connectorTypeAudioIn, module);
    render_connector({rectangle.coord.x + rectangle.size.w - (30.0 * gZoomFactor), rectangle.coord.y + rectangle.size.h - (20.0 * gZoomFactor)}, 0, connectorTypeAudioOut, module);
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + rectangle.size.h - (20.0 * gZoomFactor)}, 1, connectorTypeAudioOut, module);
}

void render_Compress(tRectangle rectangle, tModule * module) {
    render_connector({rectangle.coord.x + rectangle.size.w - (30.0 * gZoomFactor), rectangle.coord.y + (20.0 * gZoomFactor)}, 0, connectorTypeAudioIn, module);
    render_connector({rectangle.coord.x + rectangle.size.w - (10.0 * gZoomFactor), rectangle.coord.y + (20.0 * gZoomFactor)}, 1, connectorTypeAudioIn, module);
}
    
void render_parameters(tRectangle rectangle, tModule * module) {
    module->numConnectors = 0; // Ultimately, we might want to pre-calculate this per module

    if (gModuleProperties[module->type].renderFunction != NULL) {
        gModuleProperties[module->type].renderFunction(rectangle, module);
    }
}

void calculate_module_bounds(double * xEndMax, double * yEndMax, tRectangle moduleArea) {
    tModule module       = {0};
    bool    validModule  = false;
    double  moduleHeight = 0.0;
    double  xEnd = 0.0, yEnd = 0.0;


    *xEndMax = moduleArea.size.w;
    *yEndMax = moduleArea.size.h;

    reset_walk_module();
    do{
        validModule = walk_next_module(&module);
        if (validModule && module.key.location == 1 && module.type != moduleTypeUnknown0) {
            moduleHeight = (double)gModuleProperties[module.type].height;
            //xEnd         = (module.column * MODULE_X_SPAN * gZoomFactor) + (MODULE_X_SPAN * gZoomFactor) - (MODULE_X_GAP * gZoomFactor);
            //yEnd         = (module.row * MODULE_Y_SPAN * gZoomFactor) + (moduleHeight * MODULE_Y_SPAN * gZoomFactor) - (MODULE_Y_GAP * gZoomFactor);
            xEnd         = (module.column * MODULE_X_SPAN * gZoomFactor) + ((MODULE_X_SPAN - MODULE_X_GAP) * gZoomFactor);
            yEnd         = (module.row * MODULE_Y_SPAN * gZoomFactor) + (((moduleHeight * MODULE_Y_SPAN) - MODULE_Y_GAP) * gZoomFactor);

            if (xEnd > *xEndMax) {
                *xEndMax = xEnd;
            }
            if (yEnd > *yEndMax) {
                *yEndMax = yEnd;
            }
        }
    } while (validModule);
}

void render_module(tModule * module, tRectangle moduleArea, double xScrollAmount, double yScrollAmount) {
    double moduleHeight = gModuleProperties[module->type].height;
    double xPos         = ((module->column * MODULE_X_SPAN) - xScrollAmount);
    double yPos         = ((module->row * MODULE_Y_SPAN) - yScrollAmount);
    double xWidth       = MODULE_WIDTH;
    double yHeight      = (moduleHeight * MODULE_Y_SPAN) - MODULE_Y_GAP;
    char   buff[MODULE_NAME_SIZE + 1] = {0};

    tRectangle moduleRectangle = {{moduleArea.coord.x * gZoomFactor + (xPos * gZoomFactor), moduleArea.coord.y * gZoomFactor + (yPos * gZoomFactor)}, {xWidth * gZoomFactor, yHeight * gZoomFactor}};

    set_module_colour(module->colour);
    render_rectangle_with_border(moduleRectangle);
    render_parameters(moduleRectangle, module);
    write_module(module->key, module);  // Save calculated coords

    snprintf(buff, sizeof(buff), "%s", module->name);
    set_rbga_colour(RGBA_BLACK_ON_TRANSPARENT);
    render_text({{moduleRectangle.coord.x + (5.0 * gZoomFactor), moduleRectangle.coord.y + (5.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, buff);
    // Temporary items purely for development debug
    snprintf(buff, sizeof(buff), "(%s)", gModuleProperties[module->type].name);
    render_text({{moduleRectangle.coord.x + (120.0 * gZoomFactor), moduleRectangle.coord.y + (5.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, buff);
    snprintf(buff, sizeof(buff), "%u", module->key.index);
    render_text({{moduleRectangle.coord.x + moduleRectangle.size.w - (30.0 * gZoomFactor), moduleRectangle.coord.y + (5.0 * gZoomFactor)}, {0.0, 10.0 * gZoomFactor}}, buff);
}

void render_modules(void) {
    tModule    module      = {0};
    bool       validModule = false;
    tRectangle moduleArea  = module_area();
    double     xEndMax = 0.0, yEndMax = 0.0;

    calculate_module_bounds(&xEndMax, &yEndMax, moduleArea);

    //printf("Zoom = %f yEndMax = %f module area size = %f percent = %f\n", gZoomFactor, yEndMax, moduleArea.size.h, get_scroll_bar_percent(gScrollState.yBar, gRenderHeight));
    double xScrollAmount = (get_scroll_bar_percent(gScrollState.xBar, gRenderWidth) * (xEndMax - moduleArea.size.w)) / 100.0;
    double yScrollAmount = (get_scroll_bar_percent(gScrollState.yBar, gRenderHeight) * (yEndMax - moduleArea.size.h)) / 100.0;

    reset_walk_module();
    do{
        validModule = walk_next_module(&module);
        if (validModule && module.key.location == 1 && module.type != moduleTypeUnknown0) {
            render_module(&module, moduleArea, xScrollAmount, yScrollAmount);
        }
    } while (validModule);

    // Draw background areas
    set_rbg_colour(RGB_BACKGROUND_GREY);
    render_rectangle({{0.0, moduleArea.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, moduleArea.size.h + (MODULE_MARGIN * 2.0)}});
    render_rectangle({{0.0, moduleArea.coord.y - MODULE_MARGIN}, {moduleArea.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
    render_rectangle({{moduleArea.coord.x + moduleArea.size.w, moduleArea.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, moduleArea.size.h + (MODULE_MARGIN * 2.0)}});
    render_rectangle({{0.0, moduleArea.coord.y + moduleArea.size.h}, {moduleArea.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
}

void render_cable_from_to(tCoord from, tCoord to) {
    tCoord  control = {0};
    
    if ((from.x != 0.0) && (from.y != 0.0) && (to.y != 0.0) && (to.y != 0.0)) {
        if (from.x == to.x) {
            control.x = from.x;
            control.y = fmin(from.y, to.y);
        } else {
            control.x = ((from.x + to.x) / 2.0);
            control.y = fmax(from.y, to.y) + (40.0 * gZoomFactor);
        }

        render_bezier_curve(from, control, to, (4.0 * gZoomFactor), 20);
    }
}

void render_cable(tCable * cable, tRectangle moduleArea) {
    tModule moduleFrom = {0};
    tModule moduleTo   = {0};

    if (read_module({cable->key.location, cable->key.moduleFrom}, &moduleFrom) == false) {
        printf("Couldn't find from module relating to cable\n");
        exit(1);
    }
    if (read_module({cable->key.location, cable->key.moduleTo}, &moduleTo) == false) {
        printf("Couldn't find to module relating to cable\n");
        exit(1);
    }

    set_rbg_colour(cableColourMap[cable->colour]);
    
    // Could be control or audio cable, but cable struct doesn't give us that info, so try both. Might be a better way of dealing with this
    render_cable_from_to(moduleFrom.connector[cable->key.connectorFrom][connectorTypeControlOut], moduleTo.connector[cable->key.connectorTo][connectorTypeControlIn]);
    render_cable_from_to(moduleFrom.connector[cable->key.connectorFrom][connectorTypeAudioOut], moduleTo.connector[cable->key.connectorTo][connectorTypeAudioIn]);
}

void render_cables(void) {
    tCable     cable      = {0};
    bool       validCable = false;
    tRectangle moduleArea = module_area();

    reset_walk_cable();
    do{
        validCable = walk_next_cable(&cable);
        if (validCable && cable.key.location == 1) {
            render_cable(&cable, moduleArea);
        }
    } while (validCable);
}

#ifdef __cplusplus
}
#endif
