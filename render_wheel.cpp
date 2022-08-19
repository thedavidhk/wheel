#include <stdio.h>
#include "render_wheel.h"

static Color
complement(Color c);

static void
debug_draw_point(Framebuffer fb, v2 p, real32 radius, Color color);

static void
draw_line(Framebuffer fb, v2 a, v2 b, Color color);

static void
draw_triangle_wireframe(Framebuffer fb, v2 *p, Color color, uint32 thickness);

static void
draw_line(Framebuffer fb, v2 a, v2 b, Color color, uint32 thickness);

static void
draw_flat_bottom_triangle(Framebuffer fb, v2 *p, Color color);

static void
draw_flat_top_triangle(Framebuffer fb, const v2 *p, Color color);

v2
world_to_screen_space(v2 coord, const Camera &camera) {
    v2 offset = {camera.width / 2.0f, camera.height / 2.0f};
    return (coord - camera.pos) * camera.scale + offset;
}

v2
screen_to_world_space(v2 coord, const Camera &camera) {
    v2 offset = {camera.width / 2.0f, camera.height / 2.0f};
    return (coord - offset) / camera.scale + camera.pos;
}

void
draw_bitmap(Bitmap bmp, Framebuffer fb, uint32 startx, uint32 starty) {
    for (uint32 y = 0; y < bmp.height; y++) {
        for (uint32 x = 0; x < bmp.width; x++) {
            fb.data[startx + x + (starty + y) * fb.width] = bmp.pixels[x + (bmp.height - y - 1) * bmp.width];
        }
    }
}

void
draw_bitmap_alpha(Bitmap bmp, Framebuffer fb, uint32 startx, uint32 starty) {
    for (uint32 y = 0; y < bmp.height; y++) {
        for (uint32 x = 0; x < bmp.width; x++) {
            uint32 *dest = fb.data + startx + x + (starty + y) * fb.width;
            uint32 *source = bmp.pixels + x + (bmp.height - y - 1) * bmp.width;
            *dest = color_to_pixel(color_blend(pixel_to_color(*source), pixel_to_color(*dest)));
        }
    }
}

void
clear_framebuffer(Framebuffer fb, Color color) {
    uint32 pixel = color_to_pixel(color);
    for (int32 i = 0; i < fb.width * fb.height; i ++) {
        fb.data[i] = pixel;
    }
}

uint32
color_to_pixel(Color c) {
    uint32 pixel = 0;
    pixel |= round(c.b * 255);
    pixel |= (round(c.g * 255) << 8);
    pixel |= (round(c.r * 255) << 16);
    pixel |= (round(c.a * 255) << 24);
    return pixel;
}

Color
pixel_to_color(uint32 p) {
    Color c = {};
    c.a = (real32)((0xFF000000 & p) >> 24) / 255.0f;
    c.r = (real32)((0x00FF0000 & p) >> 16) / 255.0f;
    c.g = (real32)((0x0000FF00 & p) >> 8) / 255.0f;
    c.b = (real32)((0x000000FF & p) >> 0) / 255.0f;
    return c;
}

Color
color_blend(Color top, Color bottom) {
    if (top.a >= 1.0f - EPSILON)
        return top;
    return {
        top.a * top.r + bottom.r * (1 - top.a),
        top.a * top.g + bottom.g * (1 - top.a),
        top.a * top.b + bottom.b * (1 - top.a),
        1
    };
}

Color
color_opaque(Color c) {
    return {c.r, c.g, c.b, 1};
}

Color
brighten(Color c, real32 x) {
    Color new_col = c;
    new_col.r = c.r + (1 - c.r) * x;
    new_col.g = c.g + (1 - c.g) * x;
    new_col.b = c.b + (1 - c.b) * x;
    return new_col;
}
void
debug_draw_vector(Framebuffer fb, v2 v, v2 offset, Color color) {
    if (magnitude(v) < EPSILON)
        return;
    draw_line(fb, offset, v + offset, color);
    // Arrow head
    v2 n = normalize(v);
    v2 a = v + offset - 16 * n;
    v2 arrow[3];
    arrow[0] = v + offset;
    arrow[1] = a + 8 * lnormal(n, {});
    arrow[2] = a + 8 * rnormal(n, {});
    draw_triangle(fb, arrow, color);
}


void
draw_triangle(Framebuffer fb, v2 *p, Color color) {
    qsort(p, 3, sizeof(v2), v2_compare_y);
    if (round(p[1].y) == round(p[2].y)) {
        draw_flat_bottom_triangle(fb, p, color);
    }
    else if (round(p[0].y) == round(p[1].y)) {
        draw_flat_top_triangle(fb, p, color);
    }
    else {
        v2 split = {p[0].x + (p[1].y - p[0].y) * (p[2].x - p[0].x) / (p[2].y - p[0].y), p[1].y};
        v2 tmp = p[2];
        p[2] = split;
        draw_flat_bottom_triangle(fb, p, color);
        p[2] = tmp;
        p[0] = split;
        draw_flat_top_triangle(fb, p, color);
    }
}

void
draw_mesh(const Camera &camera, Framebuffer fb, Mesh mesh, Transform t, Color color) {
    for (uint32 i = 0; i < mesh.index_count; i += 3) {
        v2 tri[3];
        for (int32 j = 0; j < 3; j++)
            tri[j] = world_to_screen_space(transform(mesh.v_buffer[mesh.i[i + j]], t), camera);
        draw_triangle(fb, tri, color);
    }
}

void
draw_mesh_wireframe(const Camera &camera, Framebuffer fb, Mesh mesh, Transform t, Color color, uint32 thickness) {
    for (uint32 i = 0; i < mesh.index_count; i += 3) {
        v2 tri[3];
        for (int32 j = 0; j < 3; j++)
            tri[j] = world_to_screen_space(transform(mesh.v_buffer[mesh.i[i + j]], t), camera);
        draw_triangle_wireframe(fb, tri, color, thickness);
    }
}

static Color
complement(Color c) {
    Color new_col = c;
    new_col.r = 1 - c.r;
    new_col.b = 1 - c.b;
    new_col.g = 1 - c.g;
    return new_col;
}

static void
debug_draw_point(Framebuffer fb, v2 p, real32 radius, Color color) {
    uint32 pixel = color_to_pixel(color);
    for (int32 y = max(0, p.y - radius); y <= min(fb.height - 1, p.y + radius); y++) {
        for (int32 x = max(0, p.x - radius); x <= min(fb.width - 1, p.x + radius); x++) {
            if (magnitude(v2{(real32)x, (real32)y} - p) <= radius)
                fb.data[x + y * fb.width] = pixel;
        }
    }
}

static void
draw_line(Framebuffer fb, v2 a, v2 b, Color color) {
    real32 dx = a.x - b.x;
    real32 dy = a.y - b.y;
    real32 step = max(abs(dx), abs(dy));
    dx /= step;
    dy /= step;
    real32 x = a.x;
    real32 y = a.y;
    int32 i = 1;
    uint32 pixel = color_to_pixel(color);
    while (i++ <= step) {
        if (x > 0 && y > 0 && x < fb.width && y < fb.height)
            fb.data[(int)x + (int)y * fb.width] = pixel;
        x -= dx;
        y -= dy;
    }
}

static void
draw_triangle_wireframe(Framebuffer fb, v2 *p, Color color, uint32 thickness) {
    draw_line(fb, p[0], p[1], color, thickness);
    draw_line(fb, p[1], p[2], color, thickness);
    draw_line(fb, p[2], p[0], color, thickness);
}

static void
draw_line(Framebuffer fb, v2 a, v2 b, Color color, uint32 thickness) {
    if (thickness < 1)
        return;
    draw_line(fb, a, b, color);
    if ((abs(a.x - b.x) > EPSILON) && (abs(b.y - a.y) < abs(b.x - a.x))) {
        uint32 wy = (thickness-1)*sqrt(pow((b.x-a.x),2)+pow((b.y-a.y),2))/(2*fabs(b.x-a.x));
        for (uint32 i = 1; i < wy; i++) {
            draw_line(fb, {a.x, a.y - i}, {b.x, b.y - i}, color);
            draw_line(fb, {a.x, a.y + i}, {b.x, b.y + i}, color);
        }
    }
    else {
        uint32 wx = (thickness-1)*sqrt(pow((b.x-a.x),2)+pow((b.y-a.y),2))/(2*fabs(b.y-a.y));
        for (uint32 i = 1; i < wx; i++) {
            draw_line(fb, {a.x - i, a.y}, {b.x - i, b.y}, color);
            draw_line(fb, {a.x + i, a.y}, {b.x + i, b.y}, color);
        }
    }
}

static void
draw_flat_bottom_triangle(Framebuffer fb, v2 *p, Color color) {
    //uint32 pixel = color_to_pixel(color);
    assert(p[0].y < p[1].y && p[0].y < p[2].y);
    assert(round(p[1].y) == round(p[2].y));
    v2 p_l = p[1].x < p[2].x ? p[1] : p[2];
    v2 p_r = p[1].x > p[2].x ? p[1] : p[2];
    real32 dx_l = (p_l.x - p[0].x) / (p_l.y - p[0].y);
    real32 dx_r = (p_r.x - p[0].x) / (p_r.y - p[0].y);
    real32 x_l = p[0].x;
    real32 x_r = x_l;
    for (int32 y = max(0, p[0].y); y <= min(p_l.y - 1, fb.height - 1); y++) {
        if (y >= 0) {
            for (int32 x = max(0, x_l); x <= min(fb.width - 1, x_r - 1); x++) {
                uint32 *old = &fb.data[x + y * fb.width];
                Color old_c = pixel_to_color(*old);
                *old = color_to_pixel(color_blend(color, old_c));
            }
            x_l += dx_l;
            x_r += dx_r;
        }
    }
}

static void
draw_flat_top_triangle(Framebuffer fb, const v2 *p, Color color) {
    //uint32 pixel = color_to_pixel(color);
    assert(p[2].y > p[0].y && p[2].y > p[1].y);
    assert(round(p[0].y) == round(p[1].y));
    v2 p_l = p[0].x < p[1].x ? p[0] : p[1];
    v2 p_r = p[0].x > p[1].x ? p[0] : p[1];
    real32 dx_l = (p_l.x - p[2].x) / (p_l.y - p[2].y);
    real32 dx_r = (p_r.x - p[2].x) / (p_r.y - p[2].y);
    real32 x_l = p_l.x;
    real32 x_r = p_r.x;
    for (int32 y = max(0, p_l.y); y <= min(p[2].y - 1, fb.height - 1); y++) {
        if (y >= 0) {
            for (int32 x = max(0, x_l); x <= min(fb.width - 1, x_r - 1); x++) {
                uint32 *old = &fb.data[x + y * fb.width];
                Color old_c = pixel_to_color(*old);
                *old = color_to_pixel(color_blend(color, old_c));
            }
            x_l += dx_l;
            x_r += dx_r;
        }
    }
}




