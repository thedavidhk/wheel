#include <stdio.h>
#include "render_wheel.h"

static v4
complement(v4 c);

static void
debug_draw_point(Framebuffer fb, v2 p, real32 radius, v4 color);

static void
debug_draw_flat_top_triangle(Framebuffer fb, const v2 *p, v4 color);

static void
debug_draw_flat_bottom_triangle(Framebuffer fb, const v2 *p, v4 color);

static void
draw_triangle_wireframe(Framebuffer fb, v2 *p, v4 color, uint32 thickness);

static void
draw_flat_bottom_triangle(Framebuffer fb, v2 *pos, v4 *color, v2 *tex_coord, Texture *texture, v4 *solid_color);

static void
draw_flat_top_triangle(Framebuffer fb, v2 *pos, v4 *color, v2 *tex_coord, Texture *texture, v4 *solid_color);

static void
vertex_shader(Vertex v, Camera camera, Transform transform, v2 *screen_pos, v4 *color, v2 *tex_coord);

static v4
fragment_shader(v4 fg_color, v4 bg_color);

static int
vertex_compare_pos_y(const void *a, const void *b);

void
draw_string_to_texture(Texture *texture, const char *str, Font f, v4 color) {
    uint32 x_offset = 0;
    uint32 line_offset = 0;
    while (*str) {
        if (*str == '\n') {
            line_offset++;
            x_offset = 0;
        }
        else {
            uint32 start_x = ((*str - f.ascii_offset) * f.cwidth) % f.bitmap.width;
            uint32 start_y = f.cheight * (((*str - f.ascii_offset) * f.cwidth) / f.bitmap.width);
            for (uint32 y = 0; y < f.cheight; y++) {
                for (uint32 x = 0; x < f.cwidth; x++) {
                    v4 col = hadamard(color,pixel_to_color(f.bitmap.pixels[start_x + x + (start_y + y) * f.bitmap.width]));
                    texture->pixels[x + x_offset + (y + line_offset * f.cheight) * texture->width] = color_to_pixel(col);
                }
            }
            x_offset += f.cwidth;
        }
        str ++;
    }
}

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
debug_draw_texture(Texture texture, Framebuffer fb, uint32 startx, uint32 starty) {
    for (uint32 y = 0; y < texture.height; y++) {
        for (uint32 x = 0; x < texture.width; x++) {
            fb.data[startx + x + (starty + y) * fb.width] = texture.pixels[x + y * texture.width];
        }
    }
}

void
debug_draw_texture_alpha(Texture texture, Framebuffer fb, uint32 startx, uint32 starty) {
    for (uint32 y = 0; y < texture.height; y++) {
        for (uint32 x = 0; x < texture.width; x++) {
            uint32 *dest = fb.data + startx + x + (starty + y) * fb.width;
            uint32 *source = texture.pixels + x + y * texture.width;
            *dest = color_to_pixel(fragment_shader(pixel_to_color(*dest), pixel_to_color(*source)));
        }
    }
}

void
clear_framebuffer(Framebuffer fb, v4 color) {
    uint32 pixel = color_to_pixel(color);
    for (int32 i = 0; i < fb.width * fb.height; i ++) {
        fb.data[i] = pixel;
    }
}

uint32
color_to_pixel(v4 c) {
    uint32 pixel = 0;
    pixel |= round(c.b * 255);
    pixel |= (round(c.g * 255) << 8);
    pixel |= (round(c.r * 255) << 16);
    pixel |= (round(c.a * 255) << 24);
    return pixel;
}

v4
pixel_to_color(uint32 p) {
    v4 c = {};
    c.a = (real32)((0xFF000000 & p) >> 24) / 255.0f;
    c.r = (real32)((0x00FF0000 & p) >> 16) / 255.0f;
    c.g = (real32)((0x0000FF00 & p) >> 8) / 255.0f;
    c.b = (real32)((0x000000FF & p) >> 0) / 255.0f;
    return c;
}

v4
color_blend(v4 top, v4 bottom) {
    if (top.a >= 1.0f - EPSILON)
        return top;
    return {
        top.a * top.r + bottom.r * (1 - top.a),
        top.a * top.g + bottom.g * (1 - top.a),
        top.a * top.b + bottom.b * (1 - top.a),
        1
    };
}

v4
color_opaque(v4 c) {
    return {c.r, c.g, c.b, 1};
}

v4
brighten(v4 c, real32 x) {
    v4 new_col = c;
    new_col.r = c.r + (1 - c.r) * x;
    new_col.g = c.g + (1 - c.g) * x;
    new_col.b = c.b + (1 - c.b) * x;
    return new_col;
}
void
debug_draw_vector(Framebuffer fb, v2 v, v2 offset, v4 color) {
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
    debug_draw_triangle(fb, arrow, color);
}

void
draw_triangle(Framebuffer fb, Vertex *v, Transform t, Camera c, Texture *texture, v4 *color) {
    v2 pos[3];
    v4 vcolor[3];
    v2 tex_coord[3];
    vertex_shader(v[0], c, t, &pos[0], &vcolor[0], &tex_coord[0]);
    vertex_shader(v[1], c, t, &pos[1], &vcolor[1], &tex_coord[1]);
    vertex_shader(v[2], c, t, &pos[2], &vcolor[2], &tex_coord[2]);
    if (pos[0].y > pos[2].y) {
        v2 temp_pos = pos[0];
        v2 temp_tex_coord = tex_coord[0];
        v4 temp_vcolor = vcolor[0];
        pos[0] = pos[2];
        tex_coord[0] = tex_coord[2];
        vcolor[0] = vcolor[2];
        pos[2] = temp_pos;
        tex_coord[2] = temp_tex_coord;;
        vcolor[2] = temp_vcolor;
    }
    if (pos[0].y > pos[1].y) {
        v2 temp_pos = pos[0];
        v2 temp_tex_coord = tex_coord[0];
        v4 temp_vcolor = vcolor[0];
        pos[0] = pos[1];
        tex_coord[0] = tex_coord[1];
        vcolor[0] = vcolor[1];
        pos[1] = temp_pos;
        tex_coord[1] = temp_tex_coord;;
        vcolor[1] = temp_vcolor;
    }
    if (pos[1].y > pos[2].y) {
        v2 temp_pos = pos[1];
        v2 temp_tex_coord = tex_coord[1];
        v4 temp_vcolor = vcolor[1];
        pos[1] = pos[2];
        tex_coord[1] = tex_coord[2];
        vcolor[1] = vcolor[2];
        pos[2] = temp_pos;
        tex_coord[2] = temp_tex_coord;;
        vcolor[2] = temp_vcolor;
    }
    if (round(pos[1].y) == round(pos[2].y)) {
        draw_flat_bottom_triangle(fb, pos, vcolor, tex_coord, texture, color);
    }
    else if (round(pos[0].y) == round(pos[1].y)) {
        draw_flat_top_triangle(fb, pos, vcolor, tex_coord, texture, color);
    }
    else {
        v2 split = {pos[0].x + (pos[1].y - pos[0].y) * (pos[2].x - pos[0].x) / (pos[2].y - pos[0].y), pos[1].y};
        real32 area2 = det(pos[0], pos[1], pos[2]);
        real32 c0 = det(split, pos[1], pos[2]) / area2;
        real32 c1 = det(split, pos[2], pos[0]) / area2;
        real32 c2 = 1.0f - c0 - c1;
        v4 split_vcolor = c0 * vcolor[0] + c1 * vcolor[1] + c2 * vcolor[2];
        v2 split_tex_coord = c0 * tex_coord[0] + c1 * tex_coord[1] + c2 * tex_coord[2];
        v2 temp_pos = pos[2];
        v4 temp_vcolor = vcolor[2];
        v2 temp_tex_coord = tex_coord[2];
        pos[2] = split;
        vcolor[2] = split_vcolor;
        tex_coord[2] = split_tex_coord;
        draw_flat_bottom_triangle(fb, pos, vcolor, tex_coord, texture, color);
        pos[2] = temp_pos;
        vcolor[2] = temp_vcolor;
        tex_coord[2] = temp_tex_coord;
        pos[0] = split;
        vcolor[0] = split_vcolor;
        tex_coord[0] = split_tex_coord;
        draw_flat_top_triangle(fb, pos, vcolor, tex_coord, texture, color);
    }
}


void
debug_draw_triangle(Framebuffer fb, v2 *p, v4 color) {
    qsort(p, 3, sizeof(v2), v2_compare_y);
    if (round(p[1].y) == round(p[2].y)) {
        debug_draw_flat_bottom_triangle(fb, p, color);
    }
    else if (round(p[0].y) == round(p[1].y)) {
        debug_draw_flat_top_triangle(fb, p, color);
    }
    else {
        v2 split = {p[0].x + (p[1].y - p[0].y) * (p[2].x - p[0].x) / (p[2].y - p[0].y), p[1].y};
        v2 tmp = p[2];
        p[2] = split;
        debug_draw_flat_bottom_triangle(fb, p, color);
        p[2] = tmp;
        p[0] = split;
        debug_draw_flat_top_triangle(fb, p, color);
    }
}

void
draw_mesh(const Camera &camera, Framebuffer fb, Mesh mesh, Transform t, Texture *texture, v4 *color) {
    for (uint32 i = 0; i < mesh.index_count; i += 3) {
        Vertex tri[3];
        tri[0] = mesh.v_buffer[mesh.i[i + 0]];
        tri[1] = mesh.v_buffer[mesh.i[i + 1]];
        tri[2] = mesh.v_buffer[mesh.i[i + 2]];
        draw_triangle(fb, tri, t, camera, texture, color);
    }
}

void
draw_mesh_wireframe(const Camera &camera, Framebuffer fb, Mesh mesh, Transform t, v4 color, uint32 thickness) {
    for (uint32 i = 0; i < mesh.index_count; i += 3) {
        v2 tri[3];
        for (int32 j = 0; j < 3; j++)
            tri[j] = world_to_screen_space(transform(mesh.v_buffer[mesh.i[i + j]].coord, t), camera);
        draw_triangle_wireframe(fb, tri, color, thickness);
    }
}

static v4
complement(v4 c) {
    v4 new_col = c;
    new_col.r = 1 - c.r;
    new_col.b = 1 - c.b;
    new_col.g = 1 - c.g;
    return new_col;
}

static void
debug_draw_point(Framebuffer fb, v2 p, real32 radius, v4 color) {
    uint32 pixel = color_to_pixel(color);
    for (int32 y = max(0, p.y - radius); y <= min(fb.height - 1, p.y + radius); y++) {
        for (int32 x = max(0, p.x - radius); x <= min(fb.width - 1, p.x + radius); x++) {
            if (magnitude(v2{(real32)x, (real32)y} - p) <= radius)
                fb.data[x + y * fb.width] = pixel;
        }
    }
}

void
draw_line(Framebuffer fb, v2 a, v2 b, v4 color) {
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
draw_triangle_wireframe(Framebuffer fb, v2 *p, v4 color, uint32 thickness) {
    draw_line(fb, p[0], p[1], color, thickness);
    draw_line(fb, p[1], p[2], color, thickness);
    draw_line(fb, p[2], p[0], color, thickness);
}

void
draw_line(Framebuffer fb, v2 a, v2 b, v4 color, uint32 thickness) {
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
debug_draw_flat_bottom_triangle(Framebuffer fb, const v2 *p, v4 color) {
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
                v4 old_c = pixel_to_color(*old);
                *old = color_to_pixel(color_blend(color, old_c));
            }
            x_l += dx_l;
            x_r += dx_r;
        }
    }
}


static void
draw_fragment(Framebuffer fb, uint32 x, uint32 y, v2 *pos, v4 *vcolor, Texture *texture, v2 *tex_coord, real32 area2, v4 *solid_color) {
    v2 p = {(real32) x, (real32) y};
    real32 c0 = det(p, pos[1], pos[2]) / area2;
    real32 c1 = det(p, pos[2], pos[0]) / area2;
    real32 c2 = 1.0f - c0 - c1;
    v4 fg_color;
    if (solid_color) {
        fg_color = *solid_color;
    }
    else if ((c0 - 1) * c0 <= 0 && (c1 - 1) * c1 <= 0 && (c2 - 1) * c2 <= 0) {
        if (vcolor) {
            fg_color = c0 * vcolor[0] + c1 * vcolor[1] + c2 * vcolor[2];
        }
        if (texture) {
            v2 uv = c0 * tex_coord[0] + c1 * tex_coord[1] + c2 * tex_coord[2];
            uint32 shortest_side = min(texture->width, texture->height);
            uint32 u = round(uv.x * shortest_side); // TODO: Round or floor here?
            uint32 v = round(uv.y * shortest_side);
            fg_color = color_blend(pixel_to_color(texture->pixels[u + v * texture->width]), fg_color);
        }
    }
    uint32 *old = &fb.data[x + y * fb.width];
    v4 old_c = pixel_to_color(*old);
    *old = color_to_pixel(fragment_shader(old_c, fg_color));
}

static void
draw_flat_bottom_triangle(Framebuffer fb, v2 *pos, v4 *color, v2 *tex_coord, Texture *texture, v4 *solid_color) {
    assert(pos[0].y < pos[1].y && pos[0].y < pos[2].y);
    assert(round(pos[1].y) == round(pos[2].y));
    v2 p_l = pos[1].x < pos[2].x ? pos[1] : pos[2];
    v2 p_r = pos[1].x > pos[2].x ? pos[1] : pos[2];
    real32 dx_l = (p_l.x - pos[0].x) / (p_l.y - pos[0].y);
    real32 dx_r = (p_r.x - pos[0].x) / (p_r.y - pos[0].y);
    real32 x_l = pos[0].x;
    real32 x_r = x_l;
    real32 area2 = det(pos[0], pos[1], pos[2]);
    for (int32 y = pos[0].y; y <= min(p_l.y, fb.height - 1); y++) {
        if (y >= 0) {
            for (int32 x = max(0, x_l); x <= min(fb.width - 1, x_r - 1); x++) {
                draw_fragment(fb, x, y, pos, color, texture, tex_coord, area2, solid_color);
            }
        }
        x_l += dx_l;
        x_r += dx_r;
    }
}

static void
draw_flat_top_triangle(Framebuffer fb, v2 *pos, v4 *color, v2 *tex_coord, Texture *texture, v4 *solid_color) {
    assert(pos[2].y > pos[0].y && pos[2].y > pos[1].y);
    assert(round(pos[0].y) == round(pos[1].y));
    v2 p_l = pos[0].x < pos[1].x ? pos[0] : pos[1];
    v2 p_r = pos[0].x > pos[1].x ? pos[0] : pos[1];
    real32 dx_l = (p_l.x - pos[2].x) / (p_l.y - pos[2].y);
    real32 dx_r = (p_r.x - pos[2].x) / (p_r.y - pos[2].y);
    real32 x_l = p_l.x;
    real32 x_r = p_r.x;
    real32 area2 = det(pos[0], pos[1], pos[2]);
    for (int32 y = p_l.y; y <= min(pos[2].y, fb.height - 1); y++) {
        if (y >= 0) {
            for (int32 x = max(0, x_l); x <= min(fb.width - 1, x_r - 1); x++) {
                draw_fragment(fb, x, y, pos, color, texture, tex_coord, area2, solid_color);
            }
        }
        x_l += dx_l;
        x_r += dx_r;
    }
}

static void
debug_draw_flat_top_triangle(Framebuffer fb, const v2 *p, v4 color) {
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
                v4 old_c = pixel_to_color(*old);
                *old = color_to_pixel(color_blend(color, old_c));
            }
            x_l += dx_l;
            x_r += dx_r;
        }
    }
}

static void
vertex_shader(Vertex v, Camera cam, Transform t, v2 *screen_pos, v4 *color, v2 *tex_coord) {
    *screen_pos = world_to_screen_space(transform(v.coord, t), cam);
    *color = v.color;
    *tex_coord = v.tex_coord;
}

static v4
fragment_shader(v4 bg_color, v4 fg_color) {
    return color_blend(fg_color, bg_color);
}

static int
vertex_compare_pos_y(const void *a, const void *b) {
    const Vertex *va = (Vertex *)a;
    const Vertex *vb = (Vertex *)b;
    if (va->coord.y > vb->coord.y)
        return 1;
    else if (va->coord.y < vb->coord.y)
        return -1;
    else
        return 0;
}

