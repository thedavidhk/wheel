#include <stdio.h>  // for printf (debug only)
#include <float.h>  // for FLT_MAX
#include <string.h> // for memcpy and memset

#include "wheel.h"
#include "math_wheel.h"

#define DEBUG

typedef enum {
    CM_Pos              = 1 << 0,
    CM_Rotation         = 1 << 1,
    CM_Shape            = 1 << 2,
    CM_Color            = 1 << 3,
    CM_Velocity         = 1 << 4,
    CM_Force            = 1 << 5,
    CM_Mass             = 1 << 6,
    CM_Friction         = 1 << 7,
    CM_Collision        = 1 << 8,
    CM_BoxCollider      = 1 << 9
} ComponentMask; 

struct Color {
    f32 r, g, b, a;
};

struct Vertexbuffer {
    v2 *data;
    uint32 count, max_count;
};

struct Indexbuffer {
    uint32 *indices;
    uint32 count, max_count;
};

struct Camera {
    v2 pos;
    f32 scale; // pixels per meter
    uint32 width, height;
};

struct Shape {
    v2 *v_buffer;
    uint32 *i;
    v2 por;
    uint32 index_count;
};

struct Mass {
    v2 com;
    f32 value;
};

struct Collision {
    bool collided = false;
    v2 poi;
    v2 mtv;
};

struct Transform {
    v2 pos;
    v2 scale;
    f32 rot;
};

struct Movement {
    v2 v;
    v2 a;
    f32 rot_v;
    f32 rot_a;
};

struct Scene {
    Camera camera;
    uint32 player_index;
    v2 player_movement;
    uint32 hovered_entity;
    uint32 selected_entity; // TODO: turn this into a list(?)
    f64 scene_time;
    bool paused;

    Vertexbuffer *vertexbuffer;
    Indexbuffer *indexbuffer;

    // Entities
    uint32 entity_count;
    uint32 max_entity_count;
    uint32 *entities;
    Transform *transforms;
    Movement *movements;
    Shape *shapes;
    Color *colors;
    f32 *forces;
    Mass *masses;
    f32 *frictions;
    Collision *collisions;
};

struct AppState {
    Scene *current_scene;
    int32 mouse_x;
    int32 mouse_y;
    uint32 frame_count;
    f64 app_time;
};

struct AppMemory {
    uint64 size;
    void *data;
    void *free;
    AppState *as;
};

static void
clear_framebuffer(Framebuffer fb, Color color);

static void
accelerate(v2 direction, f32 force, f32 mass, Movement *m, f64 d_t);

static v2
world_to_screen_space(v2 coord, const Camera &camera);

static v2
screen_to_world_space(v2 coord, const Camera &camera);

static v2
transform(v2 v, Transform t);

static void*
get_memory(AppMemory *mem, uint64 size);

static Shape
create_polygon(Vertexbuffer *vb, Indexbuffer *ib, const v2 *verts, uint32
        count_v, const uint32 *indices, uint32 count_i);

static Shape
create_rectangle(Vertexbuffer *vb, Indexbuffer *ib, v2 min, v2 max);

static void
draw_triangle(Framebuffer fb, v2 *p, Color color);

static void
draw_shape(const Camera &camera, Framebuffer fb, Shape s, Transform t, Color color);

static void
draw_entities(Scene *scene, Framebuffer fb);

static uint32
add_entity(Scene *scene, uint32 mask);

static Collision
check_collision(Shape a, Shape b, Transform t_a, Transform t_b, Movement mov_a, Movement mov_b, f64 d_t);

static void
handle_collisions(Scene *scene, f64 d_t);

static void
resolve_collision(Collision col, Transform *t_a, Transform *t_b, Movement *mov_a, Movement *mov_b, f32 m_a, f32 m_b);

static void
resolve_collision(Collision col, Transform *t_a, Transform t_b, Movement *mov_a);

static void
draw_shape_wireframe(const Camera &camera, Framebuffer fb, Shape s, Transform t, Color color, uint32 thickness);

static uint32
color_to_pixel(Color c) {
    uint32 pixel = 0;
    pixel |= round(c.b * 255);
    pixel |= (round(c.g * 255) << 8);
    pixel |= (round(c.r * 255) << 16);
    pixel |= (round(c.a * 255) << 24);
    return pixel;
}

static Color
pixel_to_color(uint32 p) {
    Color c = {};
    c.a = (f32)((0xFF000000 & p) >> 24) / 255.0f;
    c.r = (f32)((0x00FF0000 & p) >> 16) / 255.0f;
    c.g = (f32)((0x0000FF00 & p) >> 8) / 255.0f;
    c.b = (f32)((0x000000FF & p) >> 0) / 255.0f;
    return c;
}

static Color
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

static Color
color_opaque(Color c) {
    return {c.r, c.g, c.b, 1};
}

static Color
brighten(Color c, f32 x) {
    Color new_col = c;
    new_col.r = c.r + (1 - c.r) * x;
    new_col.g = c.g + (1 - c.g) * x;
    new_col.b = c.b + (1 - c.b) * x;
    return new_col;
}

static Color
complement(Color c) {
    Color new_col = c;
    new_col.r = 1 - c.r;
    new_col.b = 1 - c.b;
    new_col.g = 1 - c.g;
    return new_col;
}

static f32
det(v2 a, v2 b, v2 c) {
    return (a.x - c.x) * (c.y - b.y) - (b.x - c.x) * (c.y - a.y);
}

static bool
is_in_triangle(v2 p, v2 a, v2 b, v2 c) {
    // NOTE: This works scene long scene triangles are defined counterclockwise. If
    // they are not necessarily defined clockwise, we need to check if all
    // determinants have the same sign which can be positive or negative.

    if (det(p, b, c) > 0)
        return false;
    else if (det(p, c, a) > 0)
        return false;
    else if (det(p, a, b) > 0)
        return false;

    return true;
}

static bool
is_in_shape(v2 p, Shape s, Transform t) {
    bool result = false;
    for (uint32 i = 0; i < s.index_count - 2; i += 3) {
        result = is_in_triangle(p,
                transform(s.v_buffer[s.i[i]], t),
                transform(s.v_buffer[s.i[i + 1]], t),
                transform(s.v_buffer[s.i[i + 2]], t));
        if (result)
            return result;
    }
    return result;
}

static uint32
get_entity_from_screen_pos(int32 x, int32 y, Scene *scene) {
    v2 pos = screen_to_world_space({(f32)x, (f32)y}, scene->camera);
    //printf("Mouse cursor in world space: (%4.2f, %4.2f)\n", pos.x, pos.y);
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if (is_in_shape(pos, scene->shapes[i], scene->transforms[i]))
            return i;
    }
    return 0;
}

static void
highlight_entity(Scene *scene, Framebuffer fb, uint32 id) {
    draw_shape(scene->camera, fb, scene->shapes[id], scene->transforms[id], brighten(scene->colors[id], 0.2));
    draw_shape_wireframe(scene->camera, fb, scene->shapes[id], scene->transforms[id], scene->colors[id], 3);
}

static void
clear_framebuffer(Framebuffer fb, Color color) {
    uint32 pixel = color_to_pixel(color);
    for (int32 i = 0; i < fb.width * fb.height; i ++) {
        fb.data[i] = pixel;
    }
}

static void
accelerate(v2 direction, f32 force, f32 mass, Movement *m, f64 d_t) {
    direction = normalize(direction);
    m->a += (direction * force/mass);
}

static v2
world_to_screen_space(v2 coord, const Camera &camera) {
    v2 offset = {camera.width / 2.0f, camera.height / 2.0f};
    return (coord - camera.pos) * camera.scale + offset;
}

static v2
screen_to_world_space(v2 coord, const Camera &camera) {
    v2 offset = {camera.width / 2.0f, camera.height / 2.0f};
    return (coord - offset) / camera.scale + camera.pos;
}

static void*
get_memory(AppMemory *mem, uint64 size) {
    if (size > mem->size - ((char *)mem->free - (char *)mem)) {
        printf("Not enough memory.");
        exit(1);
    }
    void *result = mem->free;
    mem->free = (char *)mem->free + size;
    return result;
}

static Shape
create_polygon(Vertexbuffer *vb, Indexbuffer *ib, const v2 *verts, uint32 count_v, const uint32 *indices, uint32 count_i) {
    Shape p = {};
    p.v_buffer = vb->data;
    p.index_count = count_i;
    p.i = ib->indices + ib->count;
    memcpy(vb->data + vb->count, verts, count_v * sizeof(*verts));
    memcpy(p.i, indices, count_i * sizeof(*indices));
    for (uint32 i = 0; i < count_i; i++) {
        p.i[i] += vb->count;
    }
    vb->count += count_v;
    ib->count += count_i;
    return p;
};

static Shape
create_rectangle(Vertexbuffer *vb, Indexbuffer *ib, v2 min, v2 max) {
    v2 verts[4] = {min, v2{max.x, min.y}, max, v2{min.x, max.y}};
    uint32 indices[6] = {0, 1, 2, 0, 2, 3};
    return create_polygon(vb, ib, verts, 4, indices, 6);
};

static v2
center_of_mass(Shape s) {
    v2 sum = {};
    uint32 i;
    for (i = 0; i < s.index_count; i++) {
        sum += s.v_buffer[s.i[i]];
    }
    return sum / (f32)i;
}

static void
debug_draw_point(Framebuffer fb, v2 p, f32 radius, Color color) {
    uint32 pixel = color_to_pixel(color);
    for (int32 y = max(0, p.y - radius); y <= min(fb.height - 1, p.y + radius); y++) {
        for (int32 x = max(0, p.x - radius); x <= min(fb.width - 1, p.x + radius); x++) {
            if (magnitude(v2{(f32)x, (f32)y} - p) <= radius)
                fb.data[x + y * fb.width] = pixel;
        }
    }
}

static void
draw_line(Framebuffer fb, v2 a, v2 b, Color color) {
    f32 dx = a.x - b.x;
    f32 dy = a.y - b.y;
    f32 step = max(abs(dx), abs(dy));
    dx /= step;
    dy /= step;
    f32 x = a.x;
    f32 y = a.y;
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


static void
draw_flat_bottom_triangle(Framebuffer fb, v2 *p, Color color) {
    //uint32 pixel = color_to_pixel(color);
    assert(p[0].y < p[1].y && p[0].y < p[2].y);
    assert(round(p[1].y) == round(p[2].y));

    v2 p_l = p[1].x < p[2].x ? p[1] : p[2];
    v2 p_r = p[1].x > p[2].x ? p[1] : p[2];

    f32 dx_l = (p_l.x - p[0].x) / (p_l.y - p[0].y);
    f32 dx_r = (p_r.x - p[0].x) / (p_r.y - p[0].y);
    f32 x_l = p[0].x;
    f32 x_r = x_l;
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

    f32 dx_l = (p_l.x - p[2].x) / (p_l.y - p[2].y);
    f32 dx_r = (p_r.x - p[2].x) / (p_r.y - p[2].y);
    f32 x_l = p_l.x;
    f32 x_r = p_r.x;
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

static void
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

static void
draw_triangle_wireframe(Framebuffer fb, v2 *p, Color color, uint32 thickness) {
    draw_line(fb, p[0], p[1], color, thickness);
    draw_line(fb, p[1], p[2], color, thickness);
    draw_line(fb, p[2], p[0], color, thickness);
}

static void
draw_shape(const Camera &camera, Framebuffer fb, Shape s, Transform t, Color color) {
    for (uint32 i = 0; i < s.index_count; i += 3) {
        v2 tri[3];
        for (int32 j = 0; j < 3; j++)
            tri[j] = world_to_screen_space(transform(s.v_buffer[s.i[i + j]], t), camera);
        draw_triangle(fb, tri, color);
    }
}

static void
draw_shape_wireframe(const Camera &camera, Framebuffer fb, Shape s, Transform t, Color color, uint32 thickness) {
    for (uint32 i = 0; i < s.index_count; i += 3) {
        v2 tri[3];
        for (int32 j = 0; j < 3; j++)
            tri[j] = world_to_screen_space(transform(s.v_buffer[s.i[i + j]], t), camera);
        draw_triangle_wireframe(fb, tri, color, thickness);
    }
}

static void
draw_entities(Scene *scene, Framebuffer fb) {
    static constexpr int32 MASK = CM_Shape | CM_Pos | CM_Color;
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            draw_shape(scene->camera, fb, scene->shapes[i], scene->transforms[i], scene->colors[i]);

            // DEBUG
            v2 vel_screen = world_to_screen_space(scene->movements[i].v, scene->camera);
            v2 pos_screen = world_to_screen_space(transform(center_of_mass(scene->shapes[i]), scene->transforms[i]), scene->camera);
            v2 origin_screen = world_to_screen_space({}, scene->camera);
            debug_draw_vector(fb, vel_screen - origin_screen, pos_screen, color_opaque(scene->colors[i]));
        }
    }
}

static void
draw_entities_wireframe(Scene *scene, Framebuffer fb) {
    static constexpr int32 MASK = CM_Shape | CM_Pos | CM_Color;
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            draw_shape_wireframe(scene->camera, fb, scene->shapes[i], scene->transforms[i], scene->colors[i], 1);

            /*
            // DEBUG
            v2 vel_screen = world_to_screen_space(scene->movements[i].v, scene->camera);
            v2 pos_screen = world_to_screen_space(transform(center_of_mass(scene->shapes[i]), scene->transforms[i]), scene->camera);
            v2 origin_screen = world_to_screen_space({}, scene->camera);
            debug_draw_vector(fb, vel_screen - origin_screen, pos_screen, scene->colors[i]);
            */
        }
    }
}

static uint32
add_entity(Scene *scene, uint32 mask) {
    scene->entities[scene->entity_count] = mask;
    return scene->entity_count++;
}

static void
update_velocities(Scene *scene, f64 d_t) {
    static constexpr int32 MASK = (CM_Pos | CM_Velocity);
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            scene->movements[i].v += scene->movements[i].a * d_t;
            scene->movements[i].a = {};
            if (scene->entities[i] & CM_Friction) {
                scene->movements[i].v -= scene->movements[i].v * scene->frictions[i] * d_t;
                scene->movements[i].rot_v -= scene->movements[i].rot_v * scene->frictions[i] * d_t;
            }
        }
    }
}

static void
update_transforms(Scene *scene, f64 d_t) {
    static constexpr int32 MASK = (CM_Pos | CM_Velocity);
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            scene->transforms[i].pos += scene->movements[i].v * d_t;
            scene->transforms[i].rot += scene->movements[i].rot_v * d_t;
        }
    }
}

static v2
transform(v2 v, Transform t) {
    return rotate(v, {0, 0}, t.rot) + t.pos;
}

// TODO: Find a realiable way to get the collision point
Collision
check_collision(Shape a, Shape b, Transform t_a, Transform t_b, Movement mov_a, Movement mov_b, f64 d_t) {
    // TODO: Probably allocate memory to allow for a larger number of lines.
    Collision result = {};
    constexpr int32 MAX_LINES = 64;
    v2 normals[MAX_LINES];
    assert(!(a.index_count > MAX_LINES || b.index_count > MAX_LINES));

    // Get line normals scene axes
    for (uint32 i = 0; i < a.index_count - 1; i++) {
        normals[i] = lnormal(transform(a.v_buffer[a.i[i]], t_a), transform(a.v_buffer[a.i[i + 1]], t_a));
    }
    normals[a.index_count - 1] = lnormal(transform(a.v_buffer[a.i[a.index_count - 1]], t_a), transform(a.v_buffer[a.i[0]], t_a));
    for (uint32 i = 0; i < b.index_count - 1; i++) {
        normals[i + a.index_count] = lnormal(transform(b.v_buffer[b.i[i]], t_b), transform(b.v_buffer[b.i[i + 1]], t_b));
    }
    normals[a.index_count + b.index_count - 1] = lnormal(transform(b.v_buffer[b.i[b.index_count - 1]], t_b), transform(b.v_buffer[b.i[0]], t_b));

    f32 min_overlap = FLT_MAX;
    v2 min_overlap_axis;

    // Extend colliders to include origin location
    Transform t_a_ext = t_a;
    t_a_ext.pos -= mov_a.v * d_t;
    t_a_ext.rot -= mov_a.rot_v * d_t;
    Transform t_b_ext = t_b;
    t_b_ext.pos -= mov_b.v * d_t;
    t_b_ext.rot -= mov_b.rot_v * d_t;

    bool a_has_face_col, b_has_face_col;
    v2 c_a, c_a2;
    v2 c_b, c_b2;

    // Project data onto axes
    for (uint32 i = 0; i < a.index_count + b.index_count; i++) {
        f32 min_a = FLT_MAX;
        f32 max_a = -FLT_MAX;
        f32 min_b = FLT_MAX;
        f32 max_b = -FLT_MAX;
        v2 v_min_a, v_min_a2, v_max_a, v_max_a2, v_min_b, v_min_b2, v_max_b, v_max_b2;
        normals[i] = normalize(normals[i]);

        for (uint32 a_i = 0; a_i < a.index_count; a_i++) {
            v2 v = transform(a.v_buffer[a.i[a_i]], t_a);
            f32 proj = dot(v, normals[i]);
            v2 v_ext = transform(a.v_buffer[a.i[a_i]], t_a_ext);
            f32 proj_ext = dot(v_ext, normals[i]);
            if (proj < min_a + EPSILON) {
                if (proj_ext < proj + EPSILON) {
                    min_a = proj_ext;
                    v_min_a2 = v_min_a;
                    v_min_a = v_ext;
                } else {
                    min_a = proj;
                    v_min_a2 = v_min_a;
                    v_min_a = v;
                }
            }
            else if (proj_ext < min_a + EPSILON) {
                assert(proj > proj_ext);
                min_a = proj_ext;
                v_min_a2 = v_min_a;
                v_min_a = v_ext;
            }
            if (proj > max_a - EPSILON) {
                if (proj_ext > proj - EPSILON) {
                    max_a = proj_ext;
                    v_max_a2 = v_max_a;
                    v_max_a = v_ext;
                } else {
                    max_a = proj;
                    v_max_a2 = v_max_a;
                    v_max_a = v;
                }
            }
            else if (proj_ext > max_a - EPSILON) {
                assert(proj < proj_ext);
                max_a = proj_ext;
                v_max_a2 = v_max_a;
                v_max_a = v_ext;
            }
        }
        for (uint32 b_i = 0; b_i < b.index_count; b_i++) {
            v2 v = transform(b.v_buffer[b.i[b_i]], t_b);
            f32 proj = dot(v, normals[i]);
            v2 v_ext = transform(b.v_buffer[b.i[b_i]], t_b_ext);
            f32 proj_ext = dot(v_ext, normals[i]);
            if (proj < min_b + EPSILON) {
                if (proj_ext < proj + EPSILON) {
                    min_b = proj_ext;
                    v_min_b2 = v_min_b;
                    v_min_b = v_ext;
                } else {
                    min_b = proj;
                    v_min_b2 = v_min_b;
                    v_min_b = v;
                }
            }
            else if (proj_ext < min_b + EPSILON) {
                assert(proj > proj_ext);
                min_b = proj_ext;
                v_min_b2 = v_min_b;
                v_min_b = v_ext;
            }
            if (proj > max_b - EPSILON) {
                if (proj_ext > proj - EPSILON) {
                    max_b = proj_ext;
                    v_max_b2 = v_max_b;
                    v_max_b = v_ext;
                } else {
                    max_b = proj;
                    v_max_b2 = v_max_b;
                    v_max_b = v;
                }
            }
            else if (proj_ext > max_b - EPSILON) {
                assert(proj < proj_ext);
                max_b = proj_ext;
                v_max_b2 = v_max_b;
                v_max_b = v_ext;
            }
        }
        if (min_a >= max_b || max_a <= min_b)
            return result; // Does not collide


        // Determine minimum translation vector
        f32 o1 = max_a - min_b;
        f32 o2 = max_b - min_a;
        f32 o = min(o1, o2);
        if (o < min_overlap) {
            min_overlap = o;
            min_overlap_axis = normals[i];
            a_has_face_col = (i < a.index_count);
            b_has_face_col = !a_has_face_col;
        }
        if (o1 < o2) {
            c_a = v_max_a;
            c_a2 = v_max_a2;
            c_b = v_min_b;
            c_b2 = v_min_b2;
            min_overlap_axis *= -1;                
        } else {
            c_a = v_min_a;
            c_a2 = v_min_a2;
            c_b = v_max_b;
            c_b2 = v_max_b2;
        }
    }
    // ONLY AT THIS POint32 WE KNOW THAT WE HAVE A COLLISION
    // TODO: Find the collision point

    // Check for face-face collision
    if (a_has_face_col) {
        if (magnitude(c_b - c_b2) < EPSILON) {
            b_has_face_col = true;
        }
    }
    if (b_has_face_col) {
        if (magnitude(c_a - c_b2) < EPSILON) {
            a_has_face_col = true;
        }
    }
    if (a_has_face_col && b_has_face_col)
        ; // Face-face collision!

    // NOTE: There should be an easier way to find the object with the face
    // collision. The normal with the smallest overlap should belong to the
    // shape that had a face collision.
    // All that remains is to check whether the other object also had a face
    // collision.

    result.collided = true;
    //result.poi = c_point_a;
    assert(min_overlap > (-EPSILON));
    result.mtv = normalize(min_overlap_axis) * min_overlap;
    return result;
}

    static void
resolve_collision(Collision col, Transform *t_a, Transform *t_b, Movement *mov_a, Movement *mov_b,
        Mass m_a, Mass m_b)
{
    v2 dv_a = projection(mov_a->v, col.mtv);
    v2 dv_b = projection(mov_b->v, col.mtv);
    v2 dv = m_b.value * dv_b + m_a.value * dv_a;
    mov_a->v += dv / (m_a.value + m_b.value) - dv_a;
    mov_b->v += dv / (m_a.value + m_b.value) - dv_b;
    t_a->pos += col.mtv;
    t_b->pos -= col.mtv;
}

static void
resolve_collision(Collision col, Shape s_a, Transform *t_a, Transform t_b, Movement *mov_a, Mass m_a, Mass m_b) {
    v2 dv = projection(mov_a->v, col.mtv);
    //v2 com_a = transform(center_of_mass(s_a), *t_a);
    mov_a->v -= dv;
    //f32 rel_torque = (col.poi.y - com_a.y) / (2 * m_a.value * (col.poi.x - com_a.x));
    //mov_a->rot_v -= rel_torque * magnitude(dv);
    t_a->pos += col.mtv;
}

static void
handle_collisions(Scene *scene, f64 d_t) {
    static constexpr int32 MASK = CM_Pos | CM_Collision;
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & (MASK | CM_Velocity | CM_Mass)) != (MASK | CM_Velocity | CM_Mass))
            continue;
        for (uint32 j = 1; j < scene->entity_count; j++) {
            if (i == j || (scene->entities[j] & MASK) != MASK)
                continue;
            Collision col = check_collision(scene->shapes[i], scene->shapes[j],
                    scene->transforms[i], scene->transforms[j], scene->movements[i],
                    scene->movements[j], d_t); if (col.collided) {
                /*
                if (scene->entities[j] == (scene->entities[j] | CM_Velocity | CM_Mass)) {
                    resolve_collision(col, &scene->transforms[i], &scene->transforms[j], &scene->movements[i],
                            &scene->movements[j], scene->masses[i], scene->masses[j]);
                } else {
                    resolve_collision(col, scene->shapes[i], &scene->transforms[i], scene->transforms[j], &scene->movements[i],
                            scene->masses[i], scene->masses[j]);
                }
                */
            }
        }
    }
}

static Scene *
initialize_scene(AppMemory *mem) {
    Scene *scene = (Scene *)get_memory(mem, sizeof(Scene));
    scene->vertexbuffer = (Vertexbuffer *)get_memory(mem, sizeof(Vertexbuffer));
    scene->indexbuffer = (Indexbuffer *)get_memory(mem, sizeof(Indexbuffer));

    // Initialize vertex and index buffer.
    static constexpr uint32 MAX_V = 64;
    scene->vertexbuffer->data = (v2 *)get_memory(mem, MAX_V * sizeof(v2));
    scene->vertexbuffer->max_count = MAX_V;
    scene->vertexbuffer->count = 0;
    static constexpr uint32 MAX_I = 64;
    scene->indexbuffer->indices = (uint32 *)get_memory(mem, MAX_I * sizeof(uint32));
    scene->indexbuffer->max_count = MAX_I;
    scene->indexbuffer->count = 0;

    // Initialize entity components.
    scene->max_entity_count = 64;
    scene->entities = (uint32 *)get_memory(mem, scene->max_entity_count * sizeof(uint32));
    scene->transforms = (Transform *)get_memory(mem, scene->max_entity_count * sizeof(Transform));
    scene->shapes = (Shape *)get_memory(mem, scene->max_entity_count * sizeof(Shape));
    scene->colors = (Color *)get_memory(mem, scene->max_entity_count * sizeof(Color));
    scene->movements = (Movement *)get_memory(mem, scene->max_entity_count * sizeof(Movement));
    scene->forces = (f32 *)get_memory(mem, scene->max_entity_count * sizeof(f32));
    scene->masses = (Mass *)get_memory(mem, scene->max_entity_count * sizeof(Mass));
    scene->frictions = (f32 *)get_memory(mem, scene->max_entity_count * sizeof(f32));
    scene->collisions = (Collision *)get_memory(mem, scene->max_entity_count * sizeof(Collision));

    // Initialize camera.
    scene->camera.pos = {0.0f, 0.0f};
    scene->camera.scale = 100.0f;
    scene->camera.width = WIN_WIDTH;
    scene->camera.height = WIN_HEIGHT;

    add_entity(scene, 0); // NULL-Entity
    return scene;
}

AppHandle
initialize_app() {
    // TODO: This is completely arbitrary!
    static constexpr uint32 mem_size = megabytes(64);

    // Allocate game memory.
    AppMemory *mem = (AppMemory *)calloc(mem_size, sizeof(char));
    if (!mem) {
        printf("Could not allocate game memory. Quitting...\n");
        exit(1);
    }
    mem->size = mem_size;
    //mem->data = (void *)mem;
    mem->free = (void *)(mem + 1);

    // Initialize memory layout.
    //MemoryLayout *ml = (MemoryLayout *)get_memory(mem, sizeof(AppState));
    AppState *as = (AppState *)get_memory(mem, sizeof(AppState));
    mem->as = as;

    Scene *scene = initialize_scene(mem);
    as->current_scene = scene;

    Vertexbuffer *vb = scene->vertexbuffer;
    Indexbuffer *ib = scene->indexbuffer;

    // Initialize player.
    constexpr uint32 count_v = 3;
    v2 data[count_v] = {
        {-0.5, -0.5},
        { 0.5, -0.3},
        {-0.5,  0.5}
    };
    constexpr uint32 count_i = 3;
    uint32 indices[count_i] = {
        0, 1, 2
    };


    int32 scenery_mask = CM_Pos | CM_Shape | CM_Color | CM_Collision | CM_Rotation;
    uint32 wall = add_entity(scene, scenery_mask);
    scene->transforms[wall].pos = {1, 1};
    scene->shapes[wall] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->colors[wall] = {0.3, 0.7, 1, 0.8};

    uint32 floor = add_entity(scene, scenery_mask);
    scene->transforms[floor].pos = {0, 6};
    scene->transforms[floor].rot = 0.2;
    scene->shapes[floor] = create_rectangle(vb, ib, {-20, -0.5}, {20, 0.5});
    scene->colors[floor] = {1, 1, 1, 0.8};

    int32 moveable_mask = CM_Pos | CM_Shape | CM_Color | CM_Collision | CM_Velocity | CM_Mass | CM_Friction;
    uint32 pebble = add_entity(scene, moveable_mask);
    scene->transforms[pebble].pos = {6, -1};
    scene->shapes[pebble] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->colors[pebble] = {0.3, 0.7, 0.2, 0.8};
    scene->movements[pebble].v = {-10, 0};
    scene->masses[pebble].value = 1;
    scene->frictions[pebble] = 1;

    int32 player_mask = CM_Pos | CM_Shape | CM_Color | CM_Velocity | CM_Force | CM_Mass | CM_Friction | CM_Collision | CM_Rotation;
    scene->player_index = add_entity(scene, player_mask);
    scene->shapes[scene->player_index] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->colors[scene->player_index] = {1, 0.7, 0.3, 0.8};
    scene->forces[scene->player_index] = 2;
    scene->masses[scene->player_index].value = 1;
    scene->frictions[scene->player_index] = 1;

    return (AppHandle)mem;
}

void
app_update_and_render(f64 frame_time, AppHandle app, Framebuffer fb) {
    AppMemory *mem = (AppMemory *)app;
    AppState *as = mem->as;
    Scene *scene = as->current_scene;

    // PHYSICS
    f64 time_left = frame_time;
    while (time_left > 0.0) {
        f64 d_t = min(frame_time, 1.0d / SIM_RATE);
        uint32 p = scene->player_index;
        if (!scene->paused) {
            accelerate(scene->player_movement, 1.0f * scene->forces[p], scene->masses[p].value, &scene->movements[p], d_t);
            update_velocities(scene, d_t);
            handle_collisions(scene, d_t);
            update_transforms(scene, d_t);
        }
        time_left -= d_t;
    }

    // RENDER
    clear_framebuffer(fb, {0});
    draw_entities(scene, fb);
    //draw_entities_wireframe(scene, fb);
    /*
    uint32 h = scene->hovered_entity;
    if (h)
        highlight_entity(scene, fb, h);
    uint32 s = scene->selected_entity;
    if (s)
        highlight_entity(scene, fb, s);
    */

    as->app_time += frame_time;
    as->frame_count++;
    if (as->app_time > 1.0f) {
        uint32 fps = round((as->frame_count / as->app_time));
        printf("FPS: %d\n", fps);
        as->frame_count = 0;
        as->app_time = 0;
    }
    /*
    if (frame_time > 1.0f / ((double)FRAME_RATE) + 0.006f) {
        printf("Frame took too long: %6.4fs\n", frame_time);
    }
    */
}

void
key_callback(KeyBoardInput key, InputType t, AppHandle game) {
    AppState *as = ((AppMemory *)game)->as;
    Scene *scene = as->current_scene;
    v2 dir = {};
    int32 mag = 2 * (t == IT_PRESSED) - 1;
    switch (key) {
        case KEY_W:
            dir += mag * v2{ 0,-1};
            break;
        case KEY_A:
            dir += mag * v2{-1, 0};
            break;
        case KEY_S:
            dir += mag * v2{ 0, 1};
            break;
        case KEY_D:
            dir += mag * v2{ 1, 0};
            break;
        case KEY_SPACE:
            if (t == IT_PRESSED) {
                printf("Scene paused.\n");
                scene->paused = !scene->paused;
            }
            break;
        default:
            break;
    }
    scene->player_movement += dir;
}

void
mouse_button_callback(MouseButton btn, InputType t, int32 x, int32 y, AppHandle game) {
    AppState *as = ((AppMemory *)game)->as;
    Scene *scene = as->current_scene;
    switch (btn) {
    case BUTTON_1:
        if (t == IT_PRESSED) {
            scene->selected_entity = scene->hovered_entity;
        }
        else {
            scene->selected_entity = 0;
        }
        break;
    case BUTTON_4:
        scene->camera.scale *= 1.1;
        break;
    case BUTTON_5:
        scene->camera.scale *= 0.9;
        break;
    default:
        break;
    }
}

void
mouse_move_callback(int32 x, int32 y, uint32 mask, AppHandle game) {
    AppState *as = ((AppMemory *)game)->as;
    Scene *scene = as->current_scene;
    uint32 entity_under_cursor = get_entity_from_screen_pos(x, y, scene);
    if (entity_under_cursor != scene->hovered_entity) {
        scene->hovered_entity = entity_under_cursor;
    }
    v2 diff_world = screen_to_world_space({(f32)x, (f32)y}, scene->camera) - screen_to_world_space({(f32)as->mouse_x, (f32)as->mouse_y}, scene->camera);
    if (scene->selected_entity) {
    // TODO: define masks ourselves
        if (mask & 1)
            scene->transforms[scene->selected_entity].pos += diff_world;
        else if (scene->entities[scene->selected_entity] & CM_Velocity)
            scene->movements[scene->selected_entity].v += diff_world;
    }
    else if (mask & (1<<8)) {
        scene->camera.pos -= diff_world;
    }
    as->mouse_x = x;
    as->mouse_y = y;
}

