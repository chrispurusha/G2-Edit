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
#include "moduleResourcesAccess.h"
#include "utilsGraphics.h"
#include "moduleGraphics.h"

extern uint32_t gLocation;

void set_module_colour(uint32_t colour) {
    tRgb rgb = {0};

    rgb = gModuleColourMap[colour];
    set_rgb_colour(rgb);
}

void render_volume_meter(tRectangle rectangle, tVolumeType volumeType, uint32_t value) { // todo: move to utilsgraphics!?
    switch (volumeType) {
        case volumeTypeCompress:
        {
            tRectangle smallRectangle = rectangle;
            double     space          = 2.0; // Todo: Possibly make a percentage of width
            uint32_t   leds           = 10;

            smallRectangle.coord.y += space;
            smallRectangle.coord.x += space;
            smallRectangle.size.h   = (smallRectangle.size.h - (space * (double)(leds + 1))) / (double)leds;
            smallRectangle.size.w  -= space * 2;

            set_rgb_colour({0.0, 0.0, 0.0});
            render_rectangle(moduleArea, rectangle);

            for (int i = 0; i < leds; i++) {
                if ((value >> i) & 0x01) {
                    set_rgb_colour({0.0, 0.7, 0.0});
                } else {
                    set_rgb_colour({0.0, 0.3, 0.0});
                }
                render_rectangle(moduleArea, smallRectangle);
                smallRectangle.coord.y += smallRectangle.size.h + space;
            }
        }
        break;

        case volumeTypeMono:
        case volumeTypeStereo:
        {
            double scaledValue = (rectangle.size.h * value) / 128.0;

            double thresholds[] = {0.5, 0.8, 1.0};  // Green up to 50%, yellow to 80%, red the rest
            double fullHeight   = rectangle.size.h;
            tRgb   colours[3]   = {
                {0.0, 0.7, 0.0},  // Green
                {0.7, 0.7, 0.0},  // Yellow
                {0.7, 0.0, 0.0}   // Red
            };

            set_rgb_colour({0.0, 0.0, 0.0});
            render_rectangle(moduleArea, rectangle);

            for (int i = 0; i < 3; i++) {
                double segmentTop    = thresholds[i] * fullHeight;
                double segmentBottom = (i == 0) ? 0 : thresholds[i - 1] * fullHeight;
                double segmentHeight = segmentTop - segmentBottom;
                double drawHeight    = 0;

                // How much of this segment should be drawn
                if (scaledValue > segmentBottom) {
                    drawHeight = scaledValue < segmentTop
                    ? scaledValue - segmentBottom
                    : segmentHeight;

                    set_rgb_colour(colours[i]);

                    render_rectangle(
                        moduleArea,
                        {{rectangle.coord.x,
                            rectangle.coord.y + fullHeight - segmentTop + (segmentHeight - drawHeight)},
                            {rectangle.size.w, drawHeight}});
                }
            }
        }

        break;
        default:
            break;
    }
}

tRectangle render_dial(tRectangle rectangle, uint32_t value, uint32_t range) {  // Drop down into utilsGraphics?
    double angle  = 0.0;
    double radius = 0.0;
    double x      = 0;
    double y      = 0;

    radius = (rectangle.size.w / 2.0);
    x      = rectangle.coord.x + radius;
    y      = rectangle.coord.y + radius;
    angle  = value_to_angle(value, range);

    //debug
    //{
    //    char buff[256] = {0};
    //    set_rgb_colour({0.5, 0.5, 0.5});
    //    snprintf(buff, sizeof(buff), "%u", value);
    //    render_text(moduleArea, {{x, y + 20}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    //}

    set_rgb_colour({0.5, 0.5, 0.5});
    render_circle_part_angle(moduleArea, {x, y}, radius, 0.0, 360.0, 25);
    set_rgb_colour({0.0, 0.0, 0.0});
    render_radial_line(moduleArea, {x, y}, radius, angle, 2.0);
    set_rgb_colour({0.0, 0.0, 0.0});
    return render_circle_line(moduleArea, {x, y}, radius, 25, 1.0);
}

tRectangle render_dial_with_text(tCoord coord, char * label, char * buff, uint32_t value, uint32_t range) {
    double y = coord.y;

    set_rgb_colour(RGB_BLACK);

    if (label != NULL) {
        render_text(moduleArea, {{coord.x, y}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, label);
    }
    y += STANDARD_TEXT_HEIGHT;

    if (buff != NULL) {
        render_text(moduleArea, {{coord.x, y}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);
    }
    y += STANDARD_TEXT_HEIGHT;
    set_rgb_colour({0.2, 0.2, 0.2});
    return render_dial({{coord.x, y}, {FILTER_FREQ_RADIUS * 2.0, FILTER_FREQ_RADIUS * 2.0}}, value, range);
}

// This might be too generic and won't be able to use, or we add extra params!
void render_param_common(tCoord coord, tModule * module, uint32_t paramRef, uint32_t paramIndex) {
    char     buff[16] = {0};
    uint32_t paramValue = module->param[0][paramIndex].value;

    module->param[0][paramIndex].paramRef = paramRef;

    switch (paramLocationList[paramRef].type1) {
        case paramType1Freq:
        {
            double freq = 0.0;

            freq = round(13.75 * pow(2, (double)paramValue / 12.0) * 100.0) / 100.0;

            if (freq < 100) {
                snprintf(buff, sizeof(buff), "%.2fHz", freq);
            } else if (freq < 1000) {
                snprintf(buff, sizeof(buff), "%.1fHz", freq);
            } else if (freq < 10000) {
                snprintf(buff, sizeof(buff), "%.2fkHz", freq / 1000.0);
            } else {
                snprintf(buff, sizeof(buff), "%.1fkHz", freq / 1000.0);
            }
            module->param[0][paramIndex].rectangle = render_dial_with_text(coord, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range);
            break;
        }
        case paramType1Pitch:
        {
            double percent = 0.0;
            double maxVal  = 200.0;

            if (paramValue < 127) {
                percent = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
            } else {
                percent = maxVal;             // Clip
            }
            snprintf(buff, sizeof(buff), "%.1f%%", percent);
            module->param[0][paramIndex].rectangle = render_dial_with_text(coord, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range);
            break;
        }
        case paramType1CommonDial:         // Ultimately might not be a common dial, or could just be a default percent dial!?
        case paramType1Resonance:
        {
            double res    = 0.0;
            double maxVal = 100.0;

            if (paramValue < 127) {
                res = round(((double)paramValue * maxVal * 10.0) / 128.0) / 10.0;
            } else {
                res = maxVal;             // Clip
            }
            snprintf(buff, sizeof(buff), "%.1f", res);
            module->param[0][paramIndex].rectangle = render_dial_with_text(coord, (char *)paramLocationList[paramRef].label, buff, paramValue, paramLocationList[paramRef].range);
            break;
        }
        case paramType1FltMultiDb:
        case paramType1FltClassicDb:
        case paramType1PitchType:
        case paramType1FmType:
        case paramType1OffTo100KeyboardTrack:
        case paramType1OffOnKeyboardTrack:
        {
            char ** map = (char **)paramLocationList[paramRef].strMap;
            double  y   = coord.y;

            if (paramLocationList[paramRef].label != NULL) {
                set_rgb_colour(RGB_BLACK);
                render_text(moduleArea, {{coord.x, y}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, (char *)paramLocationList[paramRef].label);
                y += STANDARD_TEXT_HEIGHT;
            }
            set_rgb_colour(RGB_BACKGROUND_GREY);
            module->param[0][paramIndex].rectangle = draw_button(moduleArea, {{coord.x, y}, {largest_text_width(paramLocationList[paramRef].range, map, STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}}, map[paramValue]);
            break;
        }
        case paramType1GainControl:
        {
            char * valString = "GC";

            if (paramValue != 0) {
                set_rgb_colour({0.3, 0.7, 0.3});                 // Green when ON
            } else {
                set_rgb_colour(RGB_BACKGROUND_GREY);             // Grey when OFF
            }
            module->param[0][paramIndex].rectangle = draw_button(moduleArea, {coord, get_text_width(valString, STANDARD_BUTTON_TEXT_HEIGHT), STANDARD_BUTTON_TEXT_HEIGHT}, valString);
            break;
        }
        case paramType1Bypass:
        {
            module->param[0][paramIndex].rectangle = draw_power_button(moduleArea, {coord, {BYPASS_BUTTON_WIDTH, BYPASS_BUTTON_HEIGHT}}, paramValue != 0);
            return;
        }
        case paramType1Enable:
        {
            module->param[0][paramIndex].rectangle = draw_power_button(moduleArea, {coord, {BYPASS_BUTTON_WIDTH, BYPASS_BUTTON_HEIGHT}}, paramValue != 0);
            return;
        }
        case paramType1Exp:
        {
            module->param[0][paramIndex].rectangle = draw_power_button(moduleArea, {coord, {BYPASS_BUTTON_WIDTH, BYPASS_BUTTON_HEIGHT}}, paramValue != 0);
            return;
        }
        case paramType1Pad:
        {
            module->param[0][paramIndex].rectangle = draw_power_button(moduleArea, {coord, {BYPASS_BUTTON_WIDTH, BYPASS_BUTTON_HEIGHT}}, paramValue != 0);
            return;
        }
        default:
        {
        }
        break;
    }
}

void render_mode_common(tCoord coord, tModule * module, uint32_t modeRef, uint32_t modeIndex) {

    module->mode[0].modeRef = modeRef;

    switch (modeLocationList[modeRef].type1) {
        case paramType1OscWave:
        {
            char     buff[16] = {0};
            uint32_t modeValue = 0;
            
            snprintf(buff, sizeof(buff), "%u", modeValue);
            modeValue                    = (uint32_t)round((double)(module->mode[0].value) * (double)(MAX_PARAM_RANGE - 1)) / ((double)(modeLocationList[modeRef].range - 1));
            module->mode[modeIndex].rectangle = render_dial_with_text(coord, (char *)modeLocationList[modeRef].label, buff, modeValue, modeLocationList[modeRef].range);
            break;
        }
        default:
        {
        }
        break;
    }
}

void render_volume_common(tRectangle rectangle, tModule * module, uint32_t volumeRef, uint32_t volumeIndex) {

    module->volume.volumeRef = volumeRef;

    switch (volumeLocationList[volumeRef].volumeType) {
        case volumeTypeMono:
        {
            // Debug
            //snprintf(buff, sizeof(buff), "Vol %u", module->volume.value1);
            //set_rgb_colour({0.0, 0.0, 0.0});
            //render_text(moduleArea, {{coord.x + x_param_size_from_percent(5), coord.y}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

            render_volume_meter(rectangle, gModuleProperties[module->type].volumeType, module->volume.value1);
        }
        break;
        case volumeTypeStereo:
        {
            double space = 2.0;              // Todo: Possibly make a percentage of width

            render_volume_meter(rectangle, gModuleProperties[module->type].volumeType, module->volume.value1);
            rectangle.coord.x += (rectangle.size.w + space);
            render_volume_meter(rectangle, gModuleProperties[module->type].volumeType, module->volume.value2);
        }
        break;
        case volumeTypeCompress:
        {
            render_volume_meter(rectangle, gModuleProperties[module->type].volumeType, module->volume.value1);
        }
        break;
        default:
        {
        }
        break;
    }
}

void render_led_common(tRectangle rectangle, tModule * module, uint32_t ledRef, uint32_t ledIndex) {
    
    module->led.ledRef = ledRef;

    switch (ledLocationList[ledRef].ledType) {
        case ledTypeYes:
        {
            if (module->led.value != 0) {
                set_rgb_colour({0.0, 0.7, 0.0});
            } else{
                set_rgb_colour({0.0, 0.0, 0.0});
            }
            render_rectangle(moduleArea, rectangle);
            break;
        }
        default:
        {
        }
        break;
    }
}

void render_connector_common(tRectangle rectangle, tModule * module, tConnectorDir dir, tConnectorType type, uint32_t connectorIndex) {
    
    if (module->connector == NULL) {
        printf("No connector index %u on module index %u\n", connectorIndex, module->key.index);
        return;
    }
    module->connector[connectorIndex].coord = rectangle.coord;  // Register where we're rendering this connector, for cable connecting
    module->connector[connectorIndex].dir   = dir;    // Ultimately, should be constant in the structures, we shouldn't have to do this here
    module->connector[connectorIndex].type  = type;

    if (module->upRate) {
        type = connectorTypeAudio;
    }
    set_rgb_colour(connectorColourMap[type]);  // Note, was using "module->connector[connectorIndex].type", check that this type param is OK

    if (module->connector[connectorIndex].dir == connectorDirIn) {
        //module->connector[connectorIndex].rectangle = render_circle_part(moduleArea, {coord.x + CONNECTOR_RADIUS, coord.y + CONNECTOR_RADIUS}, CONNECTOR_RADIUS, 10.0, 0.0, 10.0);
        module->connector[connectorIndex].rectangle = render_circle_part(moduleArea, {rectangle.coord.x + rectangle.size.w, rectangle.coord.y + rectangle.size.h}, rectangle.size.w, 10.0, 0.0, 10.0);
    } else {
        //module->connector[connectorIndex].rectangle = render_rectangle(moduleArea, {{coord.x, coord.y}, {CONNECTOR_DIAMETER, CONNECTOR_DIAMETER}});
        module->connector[connectorIndex].rectangle = render_rectangle(moduleArea, {rectangle.coord, {rectangle.size.w * 2, rectangle.size.h * 2}});
    }
    set_rgb_colour(RGB_BLACK);
    render_circle_part(moduleArea, {rectangle.coord.x + rectangle.size.w, rectangle.coord.y + rectangle.size.h}, rectangle.size.w / 2.0, 10.0, 0.0, 10.0);
}

tRectangle adjust_rectangle(tRectangle base, tRectangle relative, tModule * module) {
    tRectangle result;

    result.coord.x = base.coord.x + x_param_pos_from_percent(relative.coord.x);
    result.coord.y = base.coord.y + y_param_pos_from_percent(module->type, relative.coord.y);
    result.size.w  = x_param_size_from_percent(relative.size.w);
    result.size.h  = y_param_size_from_percent(module->type, relative.size.h);

    return result;
}

void render_module_common(tRectangle rectangle, tModule * module) {
    if (module == NULL) {
        return;
    }
    uint32_t param     = 0;
    uint32_t mode      = 0;
    uint32_t connector = 0;
    uint32_t volume    = 0;
    uint32_t led       = 0;

    for (uint32_t i = module->paramIndexCache; i < array_size_param_location_list(); i++) {
        if (paramLocationList[i].moduleType == module->type) {
            if (module->gotParamIndexCache == false) {
                module->paramIndexCache    = i;
                module->gotParamIndexCache = true;
            }
            render_param_common(
                {rectangle.coord.x + x_param_pos_from_percent(paramLocationList[i].offsetX), rectangle.coord.y + y_param_pos_from_percent(module->type, paramLocationList[i].offsetY)}, module, i, param++);

            if (param >= module_param_count(module->type)) {
                break;
            }
        }
    }

    for (uint32_t i = module->modeIndexCache; i < array_size_mode_location_list(); i++) {
        if (modeLocationList[i].moduleType == module->type) {
            if (module->gotModeIndexCache == false) {
                module->modeIndexCache    = i;
                module->gotModeIndexCache = true;
            }
            render_mode_common(
                {rectangle.coord.x + x_param_pos_from_percent(modeLocationList[i].offsetX), rectangle.coord.y + y_param_pos_from_percent(module->type, modeLocationList[i].offsetY)}, module, i,
                mode++);

            if (mode >= module_mode_count(module->type)) {
                break;
            }
        }
    }

    for (uint32_t i = module->connectorIndexCache; i < array_size_connector_location_list(); i++) {
        if (connectorLocationList[i].moduleType == module->type) {
            if (module->gotConnectorIndexCache == false) {
                module->connectorIndexCache    = i;
                module->gotConnectorIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, connectorLocationList[i].rectangle, module);
            adjusted.size.h = adjusted.size.w; // We want this one to be square
            render_connector_common(adjusted, module, connectorLocationList[i].direction, connectorLocationList[i].type, connector++);

            if (connector >= module_connector_count(module->type)) {
                break;
            }
        }
    }

    for (uint32_t i = module->volumeIndexCache; i < array_size_volume_location_list(); i++) {
        if (volumeLocationList[i].moduleType == module->type) {
            if (module->gotVolumeIndexCache == false) {
                module->volumeIndexCache    = i;
                module->gotVolumeIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, volumeLocationList[i].rectangle, module);
            render_volume_common(adjusted, module, i, volume++);

            if (volume >= module_volume_count(module->type)) {
                break;
            }
        }
    }

    for (uint32_t i = module->ledIndexCache; i < array_size_led_location_list(); i++) {
        if (ledLocationList[i].moduleType == module->type) {
            if (module->gotLedIndexCache == false) {
                module->ledIndexCache    = i;
                module->gotLedIndexCache = true;
            }
            tRectangle adjusted = adjust_rectangle(rectangle, ledLocationList[i].rectangle, module);
            adjusted.size.h = adjusted.size.w; // We want this one to be square
            render_led_common(adjusted, module, i, led++);

            if (led >= module_led_count(module->type)) {
                break;
            }
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
    set_rgba_colour(RGBA_BLACK_ON_TRANSPARENT);
    render_text(moduleArea, {{moduleRectangle.coord.x + 5.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

    // Temporary items purely for development debug
    snprintf(buff, sizeof(buff), "(%s)", gModuleProperties[module->type].name);

    render_text(moduleArea, {{moduleRectangle.coord.x + 120.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

    snprintf(buff, sizeof(buff), "%u", module->key.index);
    render_text(moduleArea, {{moduleRectangle.coord.x + moduleRectangle.size.w - 20.0, moduleRectangle.coord.y + 5.0}, {BLANK_SIZE, STANDARD_TEXT_HEIGHT}}, buff);

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

        if (validModule && module.key.location == gLocation && module.type != moduleTypeUnknown0) {
            render_module(&module);
        }
    } while (validModule);

    finish_walk_module();

    // Draw background areas
    set_rgb_colour(RGB_BACKGROUND_GREY);
    render_rectangle(mainArea, {{0.0, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    render_rectangle(mainArea, {{0.0, area.coord.y - MODULE_MARGIN}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
    render_rectangle(mainArea, {{area.coord.x + area.size.w, area.coord.y - MODULE_MARGIN}, {MODULE_MARGIN, area.size.h + (MODULE_MARGIN * 2.0)}});
    render_rectangle(mainArea, {{0.0, area.coord.y + area.size.h}, {area.size.w + (MODULE_MARGIN * 2.0), MODULE_MARGIN}});
}

void render_cable_from_to(tConnector from, tConnector to) {
    tCoord control = {0};

    from.coord.x += CONNECTOR_RADIUS;
    from.coord.y += CONNECTOR_RADIUS;
    to.coord.x   += CONNECTOR_RADIUS;
    to.coord.y   += CONNECTOR_RADIUS;

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
    set_rgb_colour(cableColourMap[cable->colour]);

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

        if (validCable && cable.key.location == gLocation) {
            render_cable(&cable);
        }
    } while (validCable);

    finish_walk_cable();
}

#ifdef __cplusplus
}
#endif
