#include <stdio.h>
#include <float.h>
#include <string.h>

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

struct VertexBuffer {
    v2 *vertices;
    uint32 count, max_count;
};

struct IndexBuffer {
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

struct GameState {
    Camera camera;
    uint32 player_index;
    v2 player_movement;
    uint32 hovered_entity;
    uint32 selected_entity; // TODO: turn this into a list(?)
    int32 mouse_x;
    int32 mouse_y;
    f64 t;
    uint32 frame_count;

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

struct GameMemory {
    uint64 size;
    void *data;
    void *free;
    GameState *gs;
    VertexBuffer *vb;
    IndexBuffer *ib;
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
get_memory(GameMemory *mem, uint64 size);

static Shape
create_polygon(VertexBuffer *vb, IndexBuffer *ib, const v2 *verts, uint32
        count_v, const uint32 *indices, uint32 count_i);

static Shape
create_rectangle(VertexBuffer *vb, IndexBuffer *ib, v2 min, v2 max);

static void
draw_triangle(Framebuffer fb, v2 *p, Color color);

static void
draw_shape(const Camera &camera, Framebuffer fb, Shape s, Transform t, Color color);

static void
draw_entities(GameState *g, VertexBuffer *vb, Framebuffer fb);

static uint32
add_entity(GameState *g, uint32 mask);

static Collision
check_collision(Shape a, Shape b, Transform t_a, Transform t_b, Movement mov_a, Movement mov_b, f64 d_t);

static void
handle_collisions(GameState *g, f64 d_t);

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
    // NOTE: This works as long as triangles are defined counterclockwise. If
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
get_entity_from_screen_pos(int32 x, int32 y, GameState *g) {
    v2 pos = screen_to_world_space({(f32)x, (f32)y}, g->camera);
    //printf("Mouse cursor in world space: (%4.2f, %4.2f)\n", pos.x, pos.y);
    for (uint32 i = 1; i < g->entity_count; i++) {
        if (is_in_shape(pos, g->shapes[i], g->transforms[i]))
            return i;
    }
    return 0;
}

static void
highlight_entity(GameState *g, Framebuffer fb, uint32 id) {
    draw_shape(g->camera, fb, g->shapes[id], g->transforms[id], brighten(g->colors[id], 0.2));
    draw_shape_wireframe(g->camera, fb, g->shapes[id], g->transforms[id], g->colors[id], 3);
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
get_memory(GameMemory *mem, uint64 size) {
    if (size > mem->size - ((char *)mem->free - (char *)mem)) {
        printf("Not enough memory.");
        exit(1);
    }
    void *result = mem->free;
    mem->free = (char *)mem->free + size;
    return result;
}

static Shape
create_polygon(VertexBuffer *vb, IndexBuffer *ib, const v2 *verts, uint32 count_v, const uint32 *indices, uint32 count_i) {
    Shape p = {};
    p.v_buffer = vb->vertices;
    p.index_count = count_i;
    p.i = ib->indices + ib->count;
    memcpy(vb->vertices + vb->count, verts, count_v * sizeof(*verts));
    memcpy(p.i, indices, count_i * sizeof(*indices));
    for (uint32 i = 0; i < count_i; i++) {
        p.i[i] += vb->count;
    }
    vb->count += count_v;
    ib->count += count_i;
    return p;
};

static Shape
create_rectangle(VertexBuffer *vb, IndexBuffer *ib, v2 min, v2 max) {
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
draw_line(Framebuffer pb, v2 a, v2 b, Color color) {
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
        if (x > 0 && y > 0 && x < pb.width && y < pb.height)
            pb.data[(int)x + (int)y * pb.width] = pixel;
        x -= dx;
        y -= dy;
    }
}

static void
draw_line(Framebuffer pb, v2 a, v2 b, Color color, uint32 thickness) {
    if (thickness < 1)
        return;
    draw_line(pb, a, b, color);
    if ((abs(a.x - b.x) > EPSILON) && (abs(b.y - a.y) < abs(b.x - a.x))) {
        uint32 wy = (thickness-1)*sqrt(pow((b.x-a.x),2)+pow((b.y-a.y),2))/(2*fabs(b.x-a.x));
        for (uint32 i = 1; i < wy; i++) {
            draw_line(pb, {a.x, a.y - i}, {b.x, b.y - i}, color);
            draw_line(pb, {a.x, a.y + i}, {b.x, b.y + i}, color);
        }
    }
    else {
        uint32 wx = (thickness-1)*sqrt(pow((b.x-a.x),2)+pow((b.y-a.y),2))/(2*fabs(b.y-a.y));
        for (uint32 i = 1; i < wx; i++) {
            draw_line(pb, {a.x - i, a.y}, {b.x - i, b.y}, color);
            draw_line(pb, {a.x + i, a.y}, {b.x + i, b.y}, color);
        }
    }
}

static void
debug_draw_vector(Framebuffer pb, v2 v, v2 offset, Color color) {
    if (magnitude(v) < EPSILON)
        return;
    draw_line(pb, offset, v + offset, color);
    // Arrow head
    v2 n = normalize(v);
    v2 a = v + offset - 16 * n;
    v2 arrow[3];
    arrow[0] = v + offset;
    arrow[1] = a + 8 * lnormal(n, {});
    arrow[2] = a + 8 * rnormal(n, {});
    draw_triangle(pb, arrow, color);
}


static void
draw_flat_bottom_triangle(Framebuffer pb, v2 *p, Color color) {
    uint32 pixel = color_to_pixel(color);
    assert(p[0].y < p[1].y && p[0].y < p[2].y);
    assert(round(p[1].y) == round(p[2].y));

    v2 p_l = p[1].x < p[2].x ? p[1] : p[2];
    v2 p_r = p[1].x > p[2].x ? p[1] : p[2];

    f32 dx_l = (p_l.x - p[0].x) / (p_l.y - p[0].y);
    f32 dx_r = (p_r.x - p[0].x) / (p_r.y - p[0].y);
    f32 x_l = p[0].x;
    f32 x_r = x_l;
    for (int32 y = max(0, p[0].y); y <= min(p_l.y - 1, pb.height - 1); y++) {
        if (y >= 0) {
            for (int32 x = max(0, x_l); x <= min(pb.width - 1, x_r - 1); x++) {
                pb.data[x + y * pb.width] = pixel;
            }
            x_l += dx_l;
            x_r += dx_r;
        }
    }
}

static void
draw_flat_top_triangle(Framebuffer pb, const v2 *p, Color color) {
    uint32 pixel = color_to_pixel(color);
    assert(p[2].y > p[0].y && p[2].y > p[1].y);
    assert(round(p[0].y) == round(p[1].y));

    v2 p_l = p[0].x < p[1].x ? p[0] : p[1];
    v2 p_r = p[0].x > p[1].x ? p[0] : p[1];

    f32 dx_l = (p_l.x - p[2].x) / (p_l.y - p[2].y);
    f32 dx_r = (p_r.x - p[2].x) / (p_r.y - p[2].y);
    f32 x_l = p_l.x;
    f32 x_r = p_r.x;
    for (int32 y = max(0, p_l.y); y <= min(p[2].y - 1, pb.height - 1); y++) {
        if (y >= 0) {
            for (int32 x = max(0, x_l); x <= min(pb.width - 1, x_r - 1); x++) {
                pb.data[x + y * pb.width] = pixel;
            }
            x_l += dx_l;
            x_r += dx_r;
        }
    }
}

static void
draw_triangle(Framebuffer pb, v2 *p, Color color) {
    qsort(p, 3, sizeof(v2), v2_compare_y);
    if (round(p[1].y) == round(p[2].y)) {
        draw_flat_bottom_triangle(pb, p, color);
    }
    else if (round(p[0].y) == round(p[1].y)) {
        draw_flat_top_triangle(pb, p, color);
    }
    else {
        v2 split = {p[0].x + (p[1].y - p[0].y) * (p[2].x - p[0].x) / (p[2].y - p[0].y), p[1].y};
        v2 tmp = p[2];
        p[2] = split;
        draw_flat_bottom_triangle(pb, p, color);
        p[2] = tmp;
        p[0] = split;
        draw_flat_top_triangle(pb, p, color);
    }
}

static void
draw_triangle_wireframe(Framebuffer pb, v2 *p, Color color, uint32 thickness) {
    draw_line(pb, p[0], p[1], color, thickness);
    draw_line(pb, p[1], p[2], color, thickness);
    draw_line(pb, p[2], p[0], color, thickness);
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
draw_entities(GameState *g, VertexBuffer *vb, Framebuffer fb) {
    static constexpr int32 MASK = CM_Shape | CM_Pos | CM_Color;
    for (uint32 i = 1; i < g->entity_count; i++) {
        if ((g->entities[i] & MASK) == MASK) {
            draw_shape(g->camera, fb, g->shapes[i], g->transforms[i], g->colors[i]);
        }
    }
}

static void
draw_entities_wireframe(GameState *g, VertexBuffer *vb, Framebuffer fb) {
    static constexpr int32 MASK = CM_Shape | CM_Pos | CM_Color;
    for (uint32 i = 1; i < g->entity_count; i++) {
        if ((g->entities[i] & MASK) == MASK) {
            draw_shape_wireframe(g->camera, fb, g->shapes[i], g->transforms[i], g->colors[i], 1);

            // DEBUG
            v2 vel_screen = world_to_screen_space(g->movements[i].v, g->camera);
            v2 pos_screen = world_to_screen_space(transform(center_of_mass(g->shapes[i]), g->transforms[i]), g->camera);
            v2 origin_screen = world_to_screen_space({}, g->camera);
            debug_draw_vector(fb, vel_screen - origin_screen, pos_screen, g->colors[i]);
        }
    }
}

static uint32
add_entity(GameState *g, uint32 mask) {
    g->entities[g->entity_count] = mask;
    return g->entity_count++;
}

static void
update_velocities(GameState *g, f64 d_t) {
    static constexpr int32 MASK = (CM_Pos | CM_Velocity);
    for (uint32 i = 1; i < g->entity_count; i++) {
        if ((g->entities[i] & MASK) == MASK) {
            //if (g->entities[i] & CM_Mass)
                //g->movements[i].v += v2{0, 1} * 9.87 * d_t;
            g->movements[i].v += g->movements[i].a * d_t;
            g->movements[i].a = {};
            //g->movements[i].a = {0};
            if (g->entities[i] & CM_Friction) {
                g->movements[i].v -= g->movements[i].v * g->frictions[i] * d_t;
                g->movements[i].rot_v -= g->movements[i].rot_v * g->frictions[i] * d_t;
            }
        }
    }
}

static void
update_transforms(GameState *g, f64 d_t) {
    static constexpr int32 MASK = (CM_Pos | CM_Velocity);
    for (uint32 i = 1; i < g->entity_count; i++) {
        if ((g->entities[i] & MASK) == MASK) {
            g->transforms[i].pos += g->movements[i].v * d_t;
            g->transforms[i].rot += g->movements[i].rot_v * d_t;
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

    // Get line normals as axes
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

    // Project vertices onto axes
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
handle_collisions(GameState *g, f64 d_t) {
    static constexpr int32 MASK = CM_Pos | CM_Collision;
    for (uint32 i = 1; i < g->entity_count; i++) {
        if ((g->entities[i] & (MASK | CM_Velocity | CM_Mass)) != (MASK | CM_Velocity | CM_Mass))
            continue;
        for (uint32 j = 1; j < g->entity_count; j++) {
            if (i == j || (g->entities[j] & MASK) != MASK)
                continue;
            Collision col = check_collision(g->shapes[i], g->shapes[j],
                    g->transforms[i], g->transforms[j], g->movements[i],
                    g->movements[j], d_t); if (col.collided) {
                /*
                if (g->entities[j] == (g->entities[j] | CM_Velocity | CM_Mass)) {
                    resolve_collision(col, &g->transforms[i], &g->transforms[j], &g->movements[i],
                            &g->movements[j], g->masses[i], g->masses[j]);
                } else {
                    resolve_collision(col, g->shapes[i], &g->transforms[i], g->transforms[j], &g->movements[i],
                            g->masses[i], g->masses[j]);
                }
                */
            }
        }
    }
}

GameHandle
initializeGame() {
    // TODO: This is completely arbitrary!
    static constexpr uint32 mem_size = megabytes(64);

    // Allocate game memory.
    GameMemory *mem = (GameMemory *)calloc(mem_size, sizeof(char));
    if (!mem) {
        printf("Could not allocate game memory. Quitting...\n");
        exit(1);
    }
    mem->size = mem_size;
    //mem->data = (void *)mem;
    mem->free = (void *)(mem + 1);

    // Initialize memory layout.
    //MemoryLayout *ml = (MemoryLayout *)get_memory(mem, sizeof(GameState));
    GameState *gs = (GameState *)get_memory(mem, sizeof(GameState));
    VertexBuffer *vb = (VertexBuffer *)get_memory(mem, sizeof(VertexBuffer));
    IndexBuffer *ib = (IndexBuffer *)get_memory(mem, sizeof(IndexBuffer));
    mem->gs = gs;
    mem->vb = vb;
    mem->ib = ib;

    // Initialize vertex and index buffer.
    static constexpr uint32 MAX_V = 64;
    vb->vertices = (v2 *)get_memory(mem, MAX_V * sizeof(v2));
    vb->max_count = MAX_V;
    vb->count = 0;
    static constexpr uint32 MAX_I = 64;
    ib->indices = (uint32 *)get_memory(mem, MAX_I * sizeof(uint32));
    ib->max_count = MAX_I;
    ib->count = 0;

    // Initialize entity components.
    gs->max_entity_count = 64;
    gs->entities = (uint32 *)get_memory(mem, gs->max_entity_count * sizeof(uint32));
    gs->transforms = (Transform *)get_memory(mem, gs->max_entity_count * sizeof(Transform));
    gs->shapes = (Shape *)get_memory(mem, gs->max_entity_count * sizeof(Shape));
    gs->colors = (Color *)get_memory(mem, gs->max_entity_count * sizeof(Color));
    gs->movements = (Movement *)get_memory(mem, gs->max_entity_count * sizeof(Movement));
    gs->forces = (f32 *)get_memory(mem, gs->max_entity_count * sizeof(f32));
    gs->masses = (Mass *)get_memory(mem, gs->max_entity_count * sizeof(Mass));
    gs->frictions = (f32 *)get_memory(mem, gs->max_entity_count * sizeof(f32));
    gs->collisions = (Collision *)get_memory(mem, gs->max_entity_count * sizeof(Collision));

    // Initialize camera.
    gs->camera.pos = {0.0f, 0.0f};
    gs->camera.scale = 100.0f;
    gs->camera.width = WIN_WIDTH;
    gs->camera.height = WIN_HEIGHT;

    // Initialize player.
    constexpr uint32 count_v = 3;
    v2 vertices[count_v] = {
        {-0.5, -0.5},
        { 0.5, -0.3},
        {-0.5,  0.5}
    };
    constexpr uint32 count_i = 3;
    uint32 indices[count_i] = {
        0, 1, 2
    };

    add_entity(gs, 0); // NULL-Entity

    int32 player_mask = CM_Pos | CM_Shape | CM_Color | CM_Velocity | CM_Force | CM_Mass | CM_Friction | CM_Collision | CM_Rotation;
    gs->player_index = add_entity(gs, player_mask);

    gs->shapes[gs->player_index] = create_polygon(vb, ib, vertices, count_v, indices, count_i);
    gs->colors[gs->player_index] = {1, 0.7, 0.3, 1};
    gs->forces[gs->player_index] = 60;
    //gs->transforms[gs->player_index].rot = 0;
    gs->masses[gs->player_index].value = 1;
    gs->frictions[gs->player_index] = 25;

    int32 scenery_mask = CM_Pos | CM_Shape | CM_Color | CM_Collision | CM_Rotation;
    uint32 wall = add_entity(gs, scenery_mask);
    gs->transforms[wall].pos = {1, 1};
    gs->shapes[wall] = create_polygon(vb, ib, vertices, count_v, indices, count_i);
    gs->colors[wall] = {0.3, 0.7, 1, 1};

    // uint32 floor = add_entity(gs, scenery_mask);
    // gs->transforms[floor].pos = {0, 6};
    // gs->transforms[floor].rot = 0.2;
    // gs->shapes[floor] = create_rectangle(vb, ib, {-20, -0.5}, {20, 0.5});
    // gs->colors[floor] = 0xFFFFFFFF;

    // int32 moveable_mask = CM_Pos | CM_Shape | CM_Color | CM_Collision | CM_Velocity | CM_Mass | CM_Friction;
    // uint32 pebble = add_entity(gs, moveable_mask);
    // gs->transforms[pebble].pos = {6, -1};
    // gs->shapes[pebble] = create_polygon(vb, ib, vertices, count_v, indices, count_i);
    // gs->colors[pebble] = 0xFF44CC33;
    // gs->movements[pebble].v = {-10, 0};
    // gs->masses[pebble].value = 1;
    // gs->frictions[pebble] = 1;

    return (GameHandle)mem;
}

void
gameUpdateAndRender(f64 frame_time, GameHandle game, Framebuffer fb) {
    //MemoryLayout *ml = (MemoryLayout *)mem.data;
    GameMemory *mem = (GameMemory *)game;
    GameState *game_state = mem->gs;
    VertexBuffer *vb = mem->vb;

    /*
    // INPUT
    v2 dir = {0};
    if (input->up)
        dir += v2{0, -1};
    if (input->down)
        dir += v2{0, 1};
    if (input->left)
        dir += v2{-1, 0};
    if (input->right)
        dir += v2{1, 0};
    */
    uint32 p = game_state->player_index;

    // PHYSICS
    while (frame_time > 0.0) {
        f64 d_t = min(frame_time, 1.0d / SIM_RATE);
        accelerate(game_state->player_movement, 1.0f * game_state->forces[p], game_state->masses[p].value, &game_state->movements[p], d_t);
        update_velocities(game_state, d_t);
        handle_collisions(game_state, d_t);
        update_transforms(game_state, d_t);
        game_state->t += d_t;
        frame_time -= d_t;
    }

    // RENDER
    clear_framebuffer(fb, {0});
    draw_entities(game_state, vb, fb);
    //draw_entities_wireframe(game_state, vb, fb);
    uint32 h = game_state->hovered_entity;
    if (h)
        highlight_entity(game_state, fb, h);
    uint32 s = game_state->selected_entity;
    if (s)
        highlight_entity(game_state, fb, s);

    game_state->frame_count++;
    if (game_state->t > 1) {
        uint32 fps = round((game_state->frame_count / game_state->t));
        printf("FPS: %d\n", fps);
        game_state->frame_count = 0;
        game_state->t = 0;
    }
}

void
keyPressCallback(KeyBoardInput key, InputType t, GameHandle game) {
    GameState *g = ((GameMemory *)game)->gs;
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
        // TODO: Camera should have a proper "smooth" movement
        case KEY_UP:
            g->camera.pos -= v2{0, 0.001} * g->camera.scale;
            break;
        case KEY_DOWN:
            g->camera.pos += v2{0, 0.001} * g->camera.scale;
            break;
        case KEY_LEFT:
            g->camera.pos -= v2{0.001, 0} * g->camera.scale;
            break;
        case KEY_RIGHT:
            g->camera.pos += v2{0.001, 0} * g->camera.scale;
            break;
        default:
            break;
    }
    g->player_movement += dir;
}

void
mouseButtonCallback(MouseButton btn, InputType t, int32 x, int32 y, GameHandle game) {
    GameState *g = ((GameMemory *)game)->gs;
    switch (btn) {
    case BUTTON_1:
        if (t == IT_PRESSED) {
            g->selected_entity = g->hovered_entity;
        }
        else {
            g->selected_entity = 0;
        }
        break;
    case BUTTON_4:
        g->camera.scale *= 1.1;
        break;
    case BUTTON_5:
        g->camera.scale *= 0.9;
        break;
    default:
        break;
    }
}

void
mouseMoveCallback(int32 x, int32 y, GameHandle game) {
    GameState *g = ((GameMemory *)game)->gs;
    uint32 entity_under_cursor = get_entity_from_screen_pos(x, y, g);
    if (entity_under_cursor != g->hovered_entity) {
        g->hovered_entity = entity_under_cursor;
        //printf("Hovered entity: %d\n", g->hovered_entity);
    }
    if (g->selected_entity) {
        v2 dpos = screen_to_world_space({(f32)x, (f32)y}, g->camera) - screen_to_world_space({(f32)g->mouse_x, (f32)g->mouse_y}, g->camera);
        g->transforms[g->selected_entity].pos += dpos;
    }
    g->mouse_x = x;
    g->mouse_y = y;
}

