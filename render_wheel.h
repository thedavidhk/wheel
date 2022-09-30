#ifndef RENDER_WHEEL_H

#include "wheel.h"
#include "math_wheel.h"
#include "shape_wheel.h"
#include "mesh_wheel.h"

struct Camera {
    v2 pos;
    real32 scale; // pixels per meter
    uint32 width, height;
};

struct Texture {
    union {
        uint32 *pixels;
        uint8 *bytes;
    };
    uint32 width;
    uint32 height;
};

struct Font {
    Texture bitmap;
    uint32 cwidth;
    uint32 cheight;
    uint32 ascii_offset;
};

void
renderer_draw_shape_to_buffer(Framebuffer fb, Camera c, Shape shape, v2 p, real32 p_ang);

void
draw_line(Framebuffer fb, v2 a, v2 b, v4 color);

void
draw_line(Framebuffer fb, v2 a, v2 b, v4 color, uint32 thickness);

void
clear_framebuffer(Framebuffer fb, v4 color);

void
draw_triangle(Framebuffer fb, Vertex *v, Transform t, Camera c);

void
debug_draw_triangle(Framebuffer fb, v2 *p, v4 color);

void
draw_mesh(const Camera &camera, Framebuffer fb, Mesh mesh, Transform t, Texture *texture, v4 *color);

void
draw_mesh_wireframe(const Camera &camera, Framebuffer fb, Mesh mesh, Transform t, v4 color, uint32 thickness);

void
debug_draw_texture(Texture bmp, Framebuffer fb, uint32 startx, uint32 starty);

void
debug_draw_texture_alpha(Texture bmp, Framebuffer fb, uint32 startx, uint32 starty);

void
debug_draw_vector(Framebuffer fb, v2 v, v2 offset, v4 color);

void
draw_string_to_texture(Texture *texture, const char *str, Font f, v4 color);

uint32
color_to_pixel(v4 c);

v4
pixel_to_color(uint32 p);

v4
color_blend(v4 top, v4 bottom);

v4
color_opaque(v4 c);

v4
brighten(v4 c, real32 x);

v2
world_to_screen_space(v2 coord, const Camera &camera);

v2
screen_to_world_space(v2 coord, const Camera &camera);

#define RENDER_WHEEL_H
#endif
