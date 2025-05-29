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

#include <ft2build.h>
#include FT_FREETYPE_H
#pragma clang diagnostic pop

#include "dataBase.h"
#include "moduleResourcesAccess.h"
#include "utilsGraphics.h"

static GlyphInfo glyphInfo[MAX_GLYPH_CHAR] = {0};              // Array to store glyph metadata TODO: Not being freed!?
static GLuint    textureAtlas              = 0;                // OpenGL texture handle
static int       atlasWidth                = 1024 * 8;         // Initial atlas width
static int       atlasHeight               = 1024 * 8;         // Initial atlas height
static double    gMaxAscent                = 0.0;              // Used for dealing with preloaded text character height
static double    gMaxDescent               = 0.0;
static double    gMetricsHeight            = 0.0;
static double    gXScrollPercent           = 0.0;
static double    gYScrollPercent           = 0.0;
static double    gZoomFactor               = NO_ZOOM;
static int       gRenderWidth              = 0;
static int       gRenderHeight             = 0;

static inline double scale(double value) {
    return value * gZoomFactor;
}

static inline double global_scale(double value) {
    return value * GLOBAL_GUI_SCALE;
}

double calc_scroll_x(void) {
    tRectangle area  = module_area();
    double     value = 0.0;

    value = (gXScrollPercent * (scale((MAX_COLUMNS + 1) * MODULE_X_SPAN) - area.size.w)) / 100.0;

    if (value < 0.0) {
        value = 0.0;
    }
    return value;
}

double calc_scroll_y(void) {
    tRectangle area  = module_area();
    double     value = 0.0;

    value = (gYScrollPercent * (scale(((MAX_ROWS + 1) + (MAX_ROWS_MODULE - 1)) * MODULE_Y_SPAN) - area.size.h)) / 100.0;

    if (value < 0.0) {
        value = 0.0;
    }
    return value;
}

static tCoord scale_coord(tCoord coord) {
    return {scale(coord.x), scale(coord.y)};
}

static tSize scale_size(tSize size) {
    return {scale(size.w), scale(size.h)};
}

static tRectangle scale_rectangle(tRectangle rectangle) {
    return {scale_coord(rectangle.coord), scale_size(rectangle.size)};
}

static tCoord global_scale_coord(tCoord coord) {
    return {global_scale(coord.x), global_scale(coord.y)};
}

static tSize global_scale_size(tSize size) {
    return {global_scale(size.w), global_scale(size.h)};
}

static tRectangle global_scale_rectangle(tRectangle rectangle) {
    return {global_scale_coord(rectangle.coord), global_scale_size(rectangle.size)};
}

static tCoord adjust_to_module_area_coord(tCoord coord) {
    tRectangle area = module_area();

    coord.x += area.coord.x;
    coord.y += area.coord.y;
    return coord;
}

static tRectangle adjust_to_module_area_rectangle(tRectangle rectangle) {
    return {adjust_to_module_area_coord(rectangle.coord), rectangle.size};
}

static tCoord adjust_scroll_coord(tCoord coord) {
    coord.x -= calc_scroll_x();
    coord.y -= calc_scroll_y();
    return coord;
}

static tRectangle adjust_scroll_rectangle(tRectangle rectangle) {
    return {adjust_scroll_coord(rectangle.coord), rectangle.size};
}

static tCoord scale_scroll_adjust_coord(tCoord coord) {
    coord = scale_coord(coord);
    coord = adjust_scroll_coord(coord);
    coord = adjust_to_module_area_coord(coord);
    return coord;
}

static tRectangle scale_scroll_adjust_rectangle(tRectangle rectangle) {
    rectangle = scale_rectangle(rectangle);
    rectangle = adjust_scroll_rectangle(rectangle);
    rectangle = adjust_to_module_area_rectangle(rectangle);
    return rectangle;
}

tRectangle module_area(void) {
    double left   = MODULE_MARGIN;
    double top    = TOP_BAR_HEIGHT + MODULE_MARGIN;
    double width  = (gRenderWidth / GLOBAL_GUI_SCALE) - SCROLLBAR_WIDTH - (MODULE_MARGIN * 2.0);
    double height = (gRenderHeight / GLOBAL_GUI_SCALE) - TOP_BAR_HEIGHT - SCROLLBAR_WIDTH - (MODULE_MARGIN * 2.0);

    return {{left, top}, {width, height}};
}

static void internal_render_line(tCoord start, tCoord end, double thickness) {
    double half_thickness = thickness * 0.5;
    double dx             = end.x - start.x;
    double dy             = end.y - start.y;
    double length         = sqrt(dx * dx + dy * dy);

    if (length == 0.0) {
        return;
    }
    // Normalize direction
    double nx = dx / length;
    double ny = dy / length;

    // Perpendicular vector (for thickness)
    double px = -ny * half_thickness;
    double py = nx * half_thickness;

    // Draw the thick line as a rectangle
    glBegin(GL_TRIANGLE_STRIP);
    glVertex2f(start.x - px, start.y - py);
    glVertex2f(start.x + px, start.y + py);
    glVertex2f(end.x - px, end.y - py);
    glVertex2f(end.x + px, end.y + py);
    glEnd();
}

static void internal_render_rectangle(tRectangle rectangle) {
    if ((rectangle.size.w > 0.0) && (rectangle.size.h > 0.0)) {
        glBegin(GL_QUADS);
        glVertex2f(rectangle.coord.x, rectangle.coord.y);
        glVertex2f(rectangle.coord.x + rectangle.size.w, rectangle.coord.y);
        glVertex2f(rectangle.coord.x + rectangle.size.w, rectangle.coord.y + rectangle.size.h);
        glVertex2f(rectangle.coord.x, rectangle.coord.y + rectangle.size.h);
        glEnd();
    }
}

static void internal_render_circle_part(tCoord coord, double radius, int segments, int startSeg, int numSegs) {
    double angle = 0.0;
    double x     = 0.0;
    double y     = 0.0;
    int    i     = 0;

    // seg 0 starting point = horizontel, right

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(coord.x, coord.y);  // Center

    for (i = 0; i <= numSegs; i++) {
        angle = 2.0f * M_PI * (double)(i + startSeg) / (double)segments;
        x     = coord.x + cos(angle) * radius;
        y     = coord.y + sin(angle) * radius;
        glVertex2f(x, y);
    }

    glEnd();
}

static void internal_render_circle_line_part_angle(tCoord coord, double radius, double startAngle, double endAngle, double thickness, int numSteps) {
    const double DEG_TO_RAD = M_PI / 180.0;
    double       angle, x_inner, y_inner, x_outer, y_outer;
    double       half_thickness = thickness * 0.5;

    // Normalize angle range
    double sweep = fmod((endAngle - startAngle + 360.0), 360.0);

    if (sweep == 0) {
        return;                    // Avoid rendering nothing
    }
    glEnable(GL_LINE_SMOOTH);
    glBegin(GL_TRIANGLE_STRIP);

    for (int i = 0; i <= numSteps; i++) {
        double interpAngle = startAngle + (sweep * (double)i / (double)numSteps);
        angle = (interpAngle - 90.0) * DEG_TO_RAD;     // 0° is at the top

        // Inner edge
        x_inner = coord.x + cos(angle) * (radius - half_thickness);
        y_inner = coord.y + sin(angle) * (radius - half_thickness);

        // Outer edge
        x_outer = coord.x + cos(angle) * (radius + half_thickness);
        y_outer = coord.y + sin(angle) * (radius + half_thickness);

        glVertex2f(x_inner, y_inner);
        glVertex2f(x_outer, y_outer);
    }

    glEnd();
}

static void internal_render_text(tRectangle rectangle, char * text) {
    double scaleFactor = 0.0;
    char * ch          = NULL;

    if (text == NULL) {
        //LOG_ERROR("render_text text=NULL\n");
        return;
    }
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, textureAtlas);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    glPushMatrix();

    // Calculate scale factor based on target height
    scaleFactor = rectangle.size.h / (gMaxAscent + gMaxDescent);
    glTranslatef(rectangle.coord.x, rectangle.coord.y, 0);
    glScalef(scaleFactor, scaleFactor, 1.0);

    double xCharOffset = 0.0;

    ch = text;

    while (*ch) {
        char        character = *ch;
        GlyphInfo * glyph     = &glyphInfo[character];

        // Texture coordinates for the glyph
        double u1 = glyph->u1;
        double v1 = glyph->v1;
        double u2 = glyph->u2;
        double v2 = glyph->v2;

        // Character position and size
        double xPos = glyph->offset_x + xCharOffset;
        double yPos = (gMaxAscent - glyph->offset_y);
        double w    = glyph->width;
        double h    = glyph->height;

        // Render the character quad
        glBegin(GL_QUADS);
        glTexCoord2f(u1, v1);
        glVertex2f(xPos, yPos);                                     // Bottom-left
        glTexCoord2f(u2, v1);
        glVertex2f(xPos + w, yPos);                                 // Bottom-right
        glTexCoord2f(u2, v2);
        glVertex2f(xPos + w, yPos + h);                             // Top-right
        glTexCoord2f(u1, v2);
        glVertex2f(xPos, yPos + h);                                 // Top-left
        glEnd();

        // Adjust the position for the next character (add some space between them)
        //glTranslatef(glyph->advance_x /*+ 1.0f*/, 0, 0);
        xCharOffset += glyph->advance_x;

        ch++;
    }
    glPopMatrix();

    glDisable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

tRectangle render_line(tArea area, tCoord start, tCoord end, double thickness) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        start     = scale_scroll_adjust_coord(start);
        end       = scale_scroll_adjust_coord(end);
        thickness = scale(thickness);
    }
    retRectangle = {{0.0, 0.0}, {0.0, 0.0}};

    start     = global_scale_coord(start);
    end       = global_scale_coord(end);
    thickness = global_scale(thickness);

    internal_render_line(start, end, thickness);

    return retRectangle;
}

tRectangle render_rectangle(tArea area, tRectangle rectangle) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        rectangle = scale_scroll_adjust_rectangle(rectangle);
    }
    retRectangle = rectangle;

    rectangle = global_scale_rectangle(rectangle);

    internal_render_rectangle(rectangle);

    return retRectangle;
}

tRectangle render_rectangle_with_border(tArea area, tRectangle rectangle) {
    tRectangle retRectangle = {0};

    double     borderLineWidth = BORDER_LINE_WIDTH;

    if (area == moduleArea) {
        rectangle       = scale_scroll_adjust_rectangle(rectangle);
        borderLineWidth = scale(borderLineWidth);
    }
    retRectangle = rectangle;

    rectangle       = global_scale_rectangle(rectangle);
    borderLineWidth = global_scale(borderLineWidth);

    tRectangle line = {0};

    internal_render_rectangle(rectangle);

    set_rgb_colour(RGB_BLACK);
    line = {{rectangle.coord.x, rectangle.coord.y + rectangle.size.h - borderLineWidth}, {rectangle.size.w, borderLineWidth}};
    internal_render_rectangle(line); //Bottom
    set_rgb_colour(RGB_WHITE);
    line = {{rectangle.coord.x, rectangle.coord.y}, {borderLineWidth, rectangle.size.h}};
    internal_render_rectangle(line); //Left
    set_rgb_colour(RGB_WHITE);
    line = {{rectangle.coord.x, rectangle.coord.y}, {rectangle.size.w, borderLineWidth}};
    internal_render_rectangle(line); // Top
    set_rgb_colour(RGB_BLACK);
    line = {{rectangle.coord.x + rectangle.size.w - borderLineWidth, rectangle.coord.y}, {borderLineWidth, rectangle.size.h}};
    internal_render_rectangle(line); // Right

    return retRectangle;
}

tRectangle render_triangle(tArea area, tTriangle triangle) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        triangle.coord1    = scale_scroll_adjust_coord(triangle.coord1);
        triangle.coord2rel = scale_scroll_adjust_coord(triangle.coord2rel);
        triangle.coord3rel = scale_scroll_adjust_coord(triangle.coord3rel);
    }
    retRectangle = {{0.0, 0.0}, {0.0, 0.0}};

    triangle.coord1    = global_scale_coord(triangle.coord1);
    triangle.coord2rel = global_scale_coord(triangle.coord2rel);
    triangle.coord3rel = global_scale_coord(triangle.coord3rel);

    glBegin(GL_POLYGON);
    glVertex2f(triangle.coord1.x, triangle.coord1.y);
    glVertex2f(triangle.coord1.x + triangle.coord2rel.x, triangle.coord1.y + triangle.coord2rel.y);
    glVertex2f(triangle.coord1.x + triangle.coord3rel.x, triangle.coord1.y + triangle.coord3rel.y);
    glEnd();

    return retRectangle;
}

tRectangle render_circle_line(tArea area, tCoord coord, double radius, int segments, double thickness) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        coord     = scale_scroll_adjust_coord(coord);
        radius    = scale(radius);
        thickness = scale(thickness); // WAS OUTSIDE. Hmmmm
    }
    retRectangle = {{coord.x - radius, coord.y - radius}, {radius *2.0, radius *2.0}};

    coord     = global_scale_coord(coord);
    radius    = global_scale(radius);
    thickness = global_scale(thickness);

    const double DEG_TO_RAD     = 2.0 * M_PI / (double)segments;
    double       half_thickness = thickness * 0.5;

    glEnable(GL_LINE_SMOOTH);
    glBegin(GL_TRIANGLE_STRIP);

    for (int i = 0; i <= segments; i++) {
        double angle = i * DEG_TO_RAD;
        double cos_a = cos(angle);
        double sin_a = sin(angle);

        // Compute inner and outer edge vertices
        glVertex2f(coord.x + cos_a * (radius - half_thickness),
                   coord.y + sin_a * (radius - half_thickness));

        glVertex2f(coord.x + cos_a * (radius + half_thickness),
                   coord.y + sin_a * (radius + half_thickness));
    }

    glEnd();

    return retRectangle;
}

tRectangle render_circle_part(tArea area, tCoord coord, double radius, int segments, int startSeg, int numSegs) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        coord  = scale_scroll_adjust_coord(coord);
        radius = scale(radius);
    }
    retRectangle = {{coord.x - radius, coord.y - radius}, {radius *2.0, radius *2.0}};

    coord  = global_scale_coord(coord);
    radius = global_scale(radius);

    internal_render_circle_part(coord, radius, segments, startSeg, numSegs);

    return retRectangle;
}

tRectangle render_circle_part_angle(tArea area, tCoord coord, double radius, double startAngle, double endAngle, int numSteps) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        coord  = scale_scroll_adjust_coord(coord);
        radius = scale(radius);
    }
    retRectangle = {{coord.x - radius, coord.y - radius}, {radius *2.0, radius *2.0}};

    coord  = global_scale_coord(coord);
    radius = global_scale(radius);

    double angle = 0.0;
    double x     = 0.0;
    double y     = 0.0;
    int    i     = 0;

    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(coord.x, coord.y);  // Center of the circle

    // Handle cases where the arc spans across 0°
    if (endAngle < startAngle) {
        endAngle += 360.0;     // Ensure interpolation works correctly across 0°
    }

    for (i = 0; i <= numSteps; i++) {
        // Interpolate between startAngle and endAngle
        double interpAngle = startAngle + (endAngle - startAngle) * (double)i / (double)numSteps;

        if (interpAngle >= 360.0) {
            interpAngle -= 360.0;
        }
        // Convert to radians and adjust so 0° is at the top
        angle = (interpAngle - 90.0) * (M_PI / 180.0);

        // Compute vertex position
        x = coord.x + cos(angle) * radius;
        y = coord.y + sin(angle) * radius;
        glVertex2f(x, y);
    }

    glEnd();

    return retRectangle;
}

tRectangle render_radial_line(tArea area, tCoord coord, double radius, double angleDegrees, double thickness) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        coord     = scale_scroll_adjust_coord(coord);
        radius    = scale(radius);
        thickness = scale(thickness);
    }
    retRectangle = {{coord.x - radius, coord.y - radius}, {radius *2.0, radius *2.0}};

    coord     = global_scale_coord(coord);
    radius    = global_scale(radius);
    thickness = global_scale(thickness);

    double angle = 0.0;
    double x     = 0.0;
    double y     = 0.0;

    // Adjust so 0° is at the top
    angle = (angleDegrees - 90.0) * (M_PI / 180.0);

    // Calculate endpoint of the line
    x = coord.x + cos(angle) * radius;
    y = coord.y + sin(angle) * radius;

    // Draw the line
    //render_line(xPos, yPos, x, y, thickness);
    internal_render_line({coord.x, coord.y}, {x, y}, thickness);

    return retRectangle;
}

void set_rgb_colour(tRgb rgb) {
    glColor3f(rgb.red, rgb.green, rgb.blue);
}

void set_rgba_colour(tRgba rgba) {
    glColor4f(rgba.red, rgba.green, rgba.blue, rgba.alpha);
}

tRectangle render_bezier_curve(tArea area, tCoord start, tCoord control, tCoord end, double thickness, int segments) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        start     = scale_scroll_adjust_coord(start);
        control   = scale_scroll_adjust_coord(control);
        end       = scale_scroll_adjust_coord(end);
        thickness = scale(thickness);
    }
    retRectangle = {{0.0, 0.0}, {0.0, 0.0}};

    start     = global_scale_coord(start);
    control   = global_scale_coord(control);
    end       = global_scale_coord(end);
    thickness = global_scale(thickness);

    glBegin(GL_TRIANGLE_STRIP);

    for (int i = 0; i <= segments; i++) {
        double tx = 0.0;
        double ty = 0.0;
        double t  = (double)i / (double)segments;

        double x = (1 - t) * (1 - t) * start.x + 2 * (1 - t) * t * control.x + t * t * end.x;
        double y = (1 - t) * (1 - t) * start.y + 2 * (1 - t) * t * control.y + t * t * end.y;

        if (start.x == end.x) {
            tx = 0.0;
            ty = 1.0;
            //} else if (start.y == end.y) {  // This was causing tapering on lines
            //    tx = 1.0;
            //    ty = 0.0;
        } else {
            tx = 2 * (1 - t) * (control.x - start.x) + 2 * t * (end.x - control.x);
            ty = 2 * (1 - t) * (control.y - start.y) + 2 * t * (end.y - control.y);
        }
        double length = sqrt(tx * tx + ty * ty);
        tx /= length;
        ty /= length;

        double nx = -ty * thickness * 0.5;
        double ny = tx * thickness * 0.5;

        glVertex2f(x + nx, y + ny);
        glVertex2f(x - nx, y - ny);
    }

    glEnd();

    internal_render_circle_part(start, thickness / 2.0, 10, 0, 10);
    internal_render_circle_part(end, thickness / 2.0, 10, 0, 10);

    return retRectangle;
}

// Draw the power button symbol
tRectangle draw_power_button(tArea area, tRectangle rectangle, bool active) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        rectangle = scale_scroll_adjust_rectangle(rectangle);
    }
    retRectangle = rectangle;

    rectangle = global_scale_rectangle(rectangle);

    if (active) {
        set_rgb_colour(RGB_GREEN_ON);         // Green when ON
    } else {
        set_rgb_colour(RGB_BACKGROUND_GREY);  // Grey when OFF
    }
    internal_render_rectangle(rectangle);

    set_rgb_colour(RGB_BLACK);
    tCoord circleCentre = {rectangle.coord.x + (rectangle.size.w / 2.0), rectangle.coord.y + (rectangle.size.h / 2.0)};
    double circleRadius = (rectangle.size.h / 2.0);
    circleRadius *= 0.75;

    internal_render_circle_line_part_angle(circleCentre, circleRadius, 30.0, 330.0, rectangle.size.w * 0.1, 10);
    internal_render_line({circleCentre.x, rectangle.coord.y + (rectangle.size.h * 0.05)}, {circleCentre.x, rectangle.coord.y + (rectangle.size.h * 0.05) + (rectangle.size.h * 0.5)}, rectangle.size.w * 0.1);

    return retRectangle;
}

tRectangle draw_button(tArea area, tRectangle rectangle, char * text) { // TODO: bring setting of colour for button backgound into this function
    tRectangle retRectangle    = {0};
    double     borderLineWidth = 1.0;
    double     margin          = 2.0;
    tRectangle textRectangle   = rectangle;

    rectangle.size.w       = rectangle.size.w + (2 * margin);
    rectangle.size.h       = rectangle.size.h + (2 * margin);
    textRectangle.coord.x += margin;
    textRectangle.coord.y += margin;

    if (area == moduleArea) {
        rectangle     = scale_scroll_adjust_rectangle(rectangle);
        textRectangle = scale_scroll_adjust_rectangle(textRectangle);
    }
    retRectangle = rectangle;

    rectangle     = global_scale_rectangle(rectangle);
    textRectangle = global_scale_rectangle(textRectangle);

    internal_render_rectangle(rectangle);

    set_rgb_colour(RGB_BLACK);

    tRectangle line = {0};
    line = (tRectangle){{
                            rectangle.coord.x, rectangle.coord.y + rectangle.size.h - borderLineWidth
                        }, {rectangle.size.w, borderLineWidth}};
    internal_render_rectangle(line); // Bottom
    line = (tRectangle){{
                            rectangle.coord.x, rectangle.coord.y
                        }, {borderLineWidth, rectangle.size.h}};
    internal_render_rectangle(line); // Left
    line = (tRectangle){{
                            rectangle.coord.x, rectangle.coord.y
                        }, {rectangle.size.w, borderLineWidth}};
    internal_render_rectangle(line); // Top
    line = (tRectangle){{
                            rectangle.coord.x + rectangle.size.w - borderLineWidth, rectangle.coord.y
                        }, {borderLineWidth, rectangle.size.h}};
    internal_render_rectangle(line); // Right

    internal_render_text(textRectangle, text);

    return retRectangle;
}

tRectangle render_text(tArea area, tRectangle rectangle, char * text) {
    tRectangle retRectangle = {0};

    if (area == moduleArea) {
        rectangle = scale_scroll_adjust_rectangle(rectangle);
    }
    retRectangle = rectangle;

    rectangle = global_scale_rectangle(rectangle);

    internal_render_text(rectangle, text);

    return retRectangle;
}

bool preload_glyph_textures(const char * fontPath, double fontSize) {
    FT_Library  ftLibrary  = {0};
    FT_Face     face       = {0};
    FT_UInt     glyphIndex = {0};
    FT_Bitmap * bitmap     = NULL;
    const int   padding    = 1; // Add padding between glyphs
    int         atlasX     = 0;
    int         atlasY     = 0;
    int         rowHeight  = 0;
    int         charCode   = 0;
    int         x          = 0;
    int         y          = 0;


    if (FT_Init_FreeType(&ftLibrary)) {
        LOG_DEBUG("Failed to initialize FreeType.\n");
        return false;
    }

    if (FT_New_Face(ftLibrary, fontPath, 0, &face)) {
        LOG_DEBUG("Failed to load font: %s\n", fontPath);
        FT_Done_FreeType(ftLibrary);
        return false;
    }

    if (FT_Set_Pixel_Sizes(face, 0, fontSize)) {
        LOG_DEBUG("Failed to set font size.\n");
        FT_Done_Face(face);
        FT_Done_FreeType(ftLibrary);
        return false;
    }
    // Initialize texture atlas
    glGenTextures(1, &textureAtlas);
    glBindTexture(GL_TEXTURE_2D, textureAtlas);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Initialize max ascent and descent
    gMaxAscent  = 0.0;
    gMaxDescent = 0.0;

    for (charCode = 32; charCode < MAX_GLYPH_CHAR; charCode++) {
        glyphIndex = FT_Get_Char_Index(face, charCode);

        if (glyphIndex == 0) {
            LOG_DEBUG("Glyph index not found for character %d %c\n", charCode, charCode);
            continue;
        }

        if (FT_Load_Glyph(face, glyphIndex, FT_LOAD_DEFAULT)) {
            LOG_DEBUG("Failed to load glyph for character %c\n", charCode);
            continue;
        }

        if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL)) {
            LOG_DEBUG("Failed to render glyph for character %c\n", charCode);
            continue;
        }
        // Track the highest ascent and lowest descent
        double glyphAscent = face->glyph->bitmap_top;

        if (glyphAscent < 0.0) {
            glyphAscent = 0.0;
        }

        if (glyphAscent > gMaxAscent) {
            gMaxAscent = glyphAscent;
        }
        double glyphDescent = (double)face->glyph->bitmap.rows - (double)face->glyph->bitmap_top;

        if (glyphDescent < 0.0) {
            glyphDescent = 0.0;
        }

        if (glyphDescent > gMaxDescent) {
            gMaxDescent = glyphDescent;
        }
        gMetricsHeight = (double)face->size->metrics.height / 64.0;

        bitmap = &face->glyph->bitmap;
        int texWidth  = bitmap->width;
        int texHeight = bitmap->rows;

        if ((texWidth * texHeight * 4) > 0) {
            // RGBA Conversion
            unsigned char * rgbaBuffer = (unsigned char *)malloc(texWidth * texHeight * 4);
            memset(rgbaBuffer, 0, texWidth * texHeight * 4);

            for (y = 0; y < texHeight; ++y) {
                for (x = 0; x < texWidth; ++x) {
                    int           bufferIndex = y * bitmap->pitch + x;       // Use pitch for alignment
                    int           rgbaIndex   = (y * texWidth + x) * 4;

                    unsigned char value = (x < bitmap->width && y < bitmap->rows) ? bitmap->buffer[bufferIndex] : 0;
                    rgbaBuffer[rgbaIndex + 0] = 255;                   // Red
                    rgbaBuffer[rgbaIndex + 1] = 255;                   // Green
                    rgbaBuffer[rgbaIndex + 2] = 255;                   // Blue
                    rgbaBuffer[rgbaIndex + 3] = value;                 // Alpha
                }
            }

            // Copy RGBA buffer to the atlas texture with padding
            glTexSubImage2D(GL_TEXTURE_2D, 0, atlasX + padding, atlasY + padding, texWidth, texHeight, GL_RGBA, GL_UNSIGNED_BYTE, rgbaBuffer);
            free(rgbaBuffer);
        }
        // Store glyph information with padding
        glyphInfo[charCode].u1        = (double)(atlasX + padding) / (double)atlasWidth;
        glyphInfo[charCode].v1        = (double)(atlasY + padding) / (double)atlasHeight;
        glyphInfo[charCode].u2        = (double)(atlasX + texWidth + padding) / (double)atlasWidth;
        glyphInfo[charCode].v2        = (double)(atlasY + texHeight + padding) / (double)atlasHeight;
        glyphInfo[charCode].advance_x = ((double)face->glyph->advance.x / 64.0f) + 1.0;  // Add a small amount of extra spacing
        glyphInfo[charCode].width     = texWidth;
        glyphInfo[charCode].height    = texHeight;
        glyphInfo[charCode].offset_x  = face->glyph->bitmap_left;
        glyphInfo[charCode].offset_y  = face->glyph->bitmap_top;

        atlasX += texWidth + padding;        // Add padding to the X offset

        if (texHeight + padding > rowHeight) {
            rowHeight = texHeight + padding; // Add padding to the row height
        }
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ftLibrary);

    return true;
}

double get_char_width(char ch, double targetHeight) {
    double      width = 0.0;
    GlyphInfo * glyph = &glyphInfo[ch];

    width = glyph->advance_x;

    double scaleVal = targetHeight / (gMaxAscent + gMaxDescent);
    return width * scaleVal;
}

double get_text_width(char * text, double targetHeight) {
    if (text == NULL) {
        return 0.0;
    }
    double       width = 0.0;
    const char * ch    = text;

    while (*ch) {
        width += get_char_width(*ch, targetHeight);
        ch++;
    }
    return width;
}

double largest_text_width(int numItems, char ** text, double targetHeight) {
    if (text == NULL) {
        return 0.0;
    }
    int    i       = 0;
    double size    = 0;
    double maxSize = 0;

    for (i = 0; i < numItems; i++) {
        size = get_text_width(text[i], targetHeight);

        if (size > maxSize) {
            maxSize = size;
        }
    }

    return maxSize;
}

void free_textures(void) {
    glDeleteTextures(1, &textureAtlas);
}

// Converts normalized value [0,127] back to an angle (-135° to 135°)
double value_to_angle(uint32_t value, uint32_t range) {
    if (range < 2) {
        LOG_ERROR("Can't deal with a range of %u\n", range);
        exit(1);
    }
    return ((double)value * (135.0 * 2.0) / (double)(range - 1)) - 135.0;
}

double clamp_scroll_bar(double value, double max_value) {
    max_value /= GLOBAL_GUI_SCALE;

    double half_length = SCROLLBAR_LENGTH / 2.0;
    double min_limit   = half_length + SCROLLBAR_MARGIN;
    double max_limit   = max_value - (half_length + SCROLLBAR_MARGIN);

    if (value < min_limit) {
        return min_limit;
    }

    if (value > max_limit) {
        return max_limit;
    }
    return value;
}

double get_scroll_bar_percent(double scrollBar, double renderSize) {
    double half_length = SCROLLBAR_LENGTH / 2.0;
    double low         = half_length + SCROLLBAR_MARGIN;
    double high        = renderSize - (half_length + SCROLLBAR_MARGIN);

    return ((scrollBar - low) / (high - low)) * 100.0;
}

//double set_scroll_bar_percent(double percent, double renderSize) {
//    double half_length = SCROLLBAR_LENGTH / 2.0;
//    double low         = half_length + SCROLLBAR_MARGIN;
//    double high        = renderSize - (half_length + SCROLLBAR_MARGIN);
//
// Convert percentage back to actual position on the scrollbar
//    return low + (percent / 100.0) * (high - low);
//}

// Converts angle (-135° to 135°) to normalized value [0,127]
uint32_t angle_to_value(double angle, uint32_t range) {
    // Clamp angle within [-135, 135] range
    if (angle > 135.0 && angle < 180.0) {
        angle = 135.0;
    } else if (angle >= 180.0 && angle < 360.0 - 135.0) {
        angle = 360.0 - 135.0;
    }
    // Normalize angle to [0, 270]
    angle = fmod(angle + 135.0, 360.0);

    uint32_t value = (uint32_t)floor((angle * (double)range) / 270.0);

    if (value >= range) { // If we hit 128, we only just hit it, so decrement
        value = range - 1;
    }
    return value;
}

// Calculates angle (0° at top, increasing clockwise)
double calculate_mouse_angle(tCoord mouseCoord, tRectangle rectangle) {
    double centerX = rectangle.coord.x + (rectangle.size.w / 2.0);
    double centerY = rectangle.coord.y + (rectangle.size.h / 2.0);

    double dx = mouseCoord.x - centerX;
    double dy = mouseCoord.y - centerY;

    double angle = atan2(dx, -dy) * (180.0 / M_PI);  // 0° at top

    return (angle < 0) ? angle + 360.0 : angle;
}

bool within_rectangle(tCoord coord, tRectangle rectangle) {
    return coord.x >= rectangle.coord.x
           && coord.x <= rectangle.coord.x + rectangle.size.w
           && coord.y >= rectangle.coord.y
           && coord.y <= rectangle.coord.y + rectangle.size.h;
}

void set_x_scroll_percent(double percent) {
    gXScrollPercent = percent;
}

void set_y_scroll_percent(double percent) {
    gYScrollPercent = percent;
}

void set_zoom_factor(double zoomFactor) {
    gZoomFactor = zoomFactor;
}

double get_zoom_factor(void) {
    return gZoomFactor;
}

void set_render_width(int width) {
    gRenderWidth = width;
}

void set_render_height(int height) {
    gRenderHeight = height;
}

int get_render_width(void) {
    return gRenderWidth;
}

int get_render_height(void) {
    return gRenderHeight;
}

double scale_from_percent(double val) {
    return (val * MODULE_WIDTH) / 100;
}

tRectangle rectangle_scale_from_percent(tRectangle rectangle) {
    // Scaling is done by module width, so Y percentage may be greater than 100% in some cases

    rectangle.coord.x = scale_from_percent(rectangle.coord.x);
    rectangle.coord.y = scale_from_percent(rectangle.coord.y);
    rectangle.size.w  = scale_from_percent(rectangle.size.w);
    rectangle.size.h  = scale_from_percent(rectangle.size.h);

    return rectangle;
}

#define X_POS_FROM_PERCENT(x)    ((MODULE_WIDTH * (double)x) / 100.0)
#define Y_POS_FROM_PERCENT(x)    ((MODULE_HEIGHT * (double)x) / 100.0)

#ifdef __cplusplus
}
#endif
