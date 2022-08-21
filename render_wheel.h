#ifndef RENDER_WHEEL_H

#include "wheel.h"
#include "math_wheel.h"
#include "mesh_wheel.h"

struct Camera {
    v2 pos;
    real32 scale; // pixels per meter
    uint32 width, height;
};

struct Bitmap {
    uint32 width;
    uint32 height;
    union {
        uint32 *pixels;
        uint8 *bytes;
    };
};

void
clear_framebuffer(Framebuffer fb, v4 color);

void
draw_triangle(Framebuffer fb, Vertex *v, Transform t, Camera c);

void
debug_draw_triangle(Framebuffer fb, v2 *p, v4 color);

void
draw_mesh(const Camera &camera, Framebuffer fb, Mesh mesh, Transform t, v4 color);

void
draw_mesh_wireframe(const Camera &camera, Framebuffer fb, Mesh mesh, Transform t, v4 color, uint32 thickness);

void
draw_bitmap(Bitmap bmp, Framebuffer fb, uint32 startx, uint32 starty);

void
draw_bitmap_alpha(Bitmap bmp, Framebuffer fb, uint32 startx, uint32 starty);

void
debug_draw_vector(Framebuffer fb, v2 v, v2 offset, v4 color);

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
