#include <stdio.h>
#include <float.h>
#include <string.h>

#include "handmade.h"
#include "math_handmade.h"

typedef enum {
    CM_Pos              = 1 << 0,
    CM_Shape            = 1 << 1,
    CM_Color            = 1 << 2,
    CM_Velocity         = 1 << 3,
    CM_Actuator         = 1 << 4,
    CM_Collision        = 1 << 5,
    CM_BoxCollider      = 1 << 6
} ComponentMask; 

struct VertexBuffer {
    v2 *vertices;
    unsigned int count, max_count;
};

struct IndexBuffer {
    unsigned int *indices;
    unsigned int count, max_count;
};

struct Actuator {
    float max_acceleration, friction;
};

struct Camera {
    v2 pos;
    float scale; // pixels per meter
    unsigned int width, height;
};

struct Shape {
    v2 *v_buffer;
    unsigned int *i;
    unsigned int index_count;
};

struct Collision {
    bool collided = false;
    v2 mtv;
};

struct Player {
    v2 pos;
    Shape shape;
    unsigned int color;
    v2 velocity;
    Actuator actuator;
    Collision collision;
};

struct Scenery {
    v2 pos;
    Shape shape;
    unsigned int color;
    Collision collision;
};

struct GameState {
    Camera camera;
    unsigned int player_index;

    // Entities
    unsigned int entity_count;
    unsigned int max_entity_count;
    unsigned int *entities;
    v2 *positions;
    Shape *shapes;
    unsigned int *colors;
    v2 *velocities;
    Actuator *actuators;
    Collision *collisions;
};

struct MemoryLayout {
    GameState *gs;
    VertexBuffer *vb;
    IndexBuffer *ib;
};

static void
clear_pixel_buffer(PixelBuffer b, unsigned int color);

static void
accelerate(v2 direction, float intensity, Actuator actuator, v2 *velocity, double d_t);

static v2
world_to_screen_space(v2 coord, const Camera &camera);

static Rectangle
world_to_screen_space(Rectangle rect, const Camera &camera);

static Triangle
world_to_screen_space(Triangle t, const Camera &camera);

static void*
get_memory(GameMemory *mem, unsigned long long size);

static Shape
create_polygon(VertexBuffer *vb, IndexBuffer *ib, const v2 *verts, unsigned int
        count_v, const unsigned int *indices, unsigned int count_i);

static Shape
create_rectangle(VertexBuffer *vb, IndexBuffer *ib, v2 min, v2 max);

static bool
is_on_screen(v2 point, const Camera &camera);

static Rectangle
bounding_box(v2 *v, int count);

static bool
is_inside_triangle(v2 p, v2 a, v2 b, v2 c);

static void
draw_triangle(const Camera &camera, PixelBuffer b, unsigned int *index, v2 *vertex_buffer, v2 pos, unsigned int color);

static void
draw_shape(const Camera &camera, PixelBuffer b, Shape s, v2 *vertex_buffer, v2 pos, unsigned int color);

static void
draw_entities(GameState *g, VertexBuffer *vb, PixelBuffer b);

static unsigned int
add_entity(GameState *g, unsigned int mask);

static void
update_components(GameState *g, unsigned int entity, int mask, void *data);

static void
update_positions(GameState *g, double d_t);

static Collision
check_collision(Shape *collider_a, Shape *collider_b, v2 pos);

static void
handle_collisions(GameState *g, double d_t);

static void
resolve_collision(Collision col, v2 *pos_a, v2 *pos_b, v2 *vel_a, v2 *vel_b);

static void
resolve_collision(Collision col, v2 *pos_a, v2 *pos_b, v2 *vel_a);

static void
clear_pixel_buffer(PixelBuffer b, unsigned int color) {
    for (unsigned int i = 0; i < b.width * b.height; i ++) {
        b.data[i] = color;
    }
}

static void
accelerate(v2 direction, float intensity, Actuator actuator, v2 *velocity, double d_t) {
    direction = normalize(direction);
    *velocity += (direction * intensity * actuator.max_acceleration - (*velocity) * actuator.friction) * d_t;
}

static v2
world_to_screen_space(v2 coord, const Camera &camera) {
    v2 offset = {camera.width / 2.0f, camera.height / 2.0f};
    return (coord - camera.pos) * camera.scale + offset;
}

static Rectangle
world_to_screen_space(Rectangle rect, const Camera &camera) {
    return Rectangle(world_to_screen_space(rect.min, camera), world_to_screen_space(rect.max, camera));
}

static Triangle
world_to_screen_space(Triangle t, const Camera &camera) {
    return Triangle{world_to_screen_space(t.a, camera), world_to_screen_space(t.b, camera), world_to_screen_space(t.c, camera)};
}

static void*
get_memory(GameMemory *mem, unsigned long long size) {
    if (size > mem->size - ((char *)mem->free - (char *)mem->data)) {
        printf("Not enough memory.");
        exit(1);
    }
    void *result = mem->free;
    mem->free = (char *)mem->free + size;
    return result;
}

static Shape
create_polygon(VertexBuffer *vb, IndexBuffer *ib, const v2 *verts, unsigned int count_v, const unsigned int *indices, unsigned int count_i) {
    Shape p = {};
    p.v_buffer = vb->vertices;
    p.index_count = count_i;
    memcpy(vb->vertices + vb->count, verts, count_v * sizeof(*verts));
    vb->count += count_v;
    p.i = ib->indices + ib->count;
    memcpy(p.i, indices, count_i * sizeof(*indices));
    ib->count += count_i;
    return p;
};

static Shape
create_rectangle(VertexBuffer *vb, IndexBuffer *ib, v2 min, v2 max) {
    v2 verts[4] = {min, v2{max.x, min.y}, max, v2{min.x, max.y}};
    unsigned int indices[6] = {0, 1, 2, 0, 2, 3};
    return create_polygon(vb, ib, verts, 4, indices, 6);
};

static bool
is_on_screen(v2 point, const Camera &camera) {
    return (point.x >= 0 && point.x <= camera.width && point.y >= 0 && point.y < camera.height);
};

static Rectangle
bounding_box(v2 *v, int count) {
    v2 min = {FLT_MAX, FLT_MAX};
    v2 max = {FLT_MIN, FLT_MIN};
    for (int i = 0; i < count; i++) {
        if (v[i].x < min.x)
            min.x = v[i].x;
        if (v[i].y < min.y)
            min.y = v[i].y;
        if (v[i].x > max.x)
            max.x = v[i].x;
        if (v[i].y > max.y)
            max.y = v[i].y;
    }
    return Rectangle(min, max);
};

static bool
is_inside_triangle(v2 p, v2 a, v2 b, v2 c) {
    return (dot(p - a, rnormal(Line{a, b})) >= 0 &&
            dot(p - b, rnormal(Line{b, c})) >= 0 &&
            dot(p - c, rnormal(Line{c, a})) >= 0);
}

static void
draw_triangle(const Camera &camera, PixelBuffer b, unsigned int *index, v2 *vertex_buffer, v2 pos, unsigned int color) {
    bool off_screen = true;
    v2 tri[3];
    for (int i = 0; i < 3; i++) {
        tri[i] = world_to_screen_space(vertex_buffer[index[i]] + pos, camera);
        off_screen = off_screen && (!is_on_screen(tri[i], camera));
    }
    if (off_screen)
        return;

    Rectangle screen = Rectangle{v2{0, 0}, v2{(float)camera.width, (float)camera.height}};
    Rectangle draw_area = area_intersection(bounding_box(tri, 3), screen);
    for (int y = draw_area.min.y; y < draw_area.max.y; y++) {
        unsigned int *row = (unsigned int *)(b.data + y * b.width);
        bool drawn = false;
        for (int x = draw_area.min.x; x < draw_area.max.x; x++) {
            unsigned int *p = row + x;
            if (is_inside_triangle({(float)x, (float)y}, tri[0], tri[1], tri[2])) {
                *p = color;
                drawn = true;
            } else if (drawn) {
                break; // If row has been drawn, skip to next row
            }
        }
    }
};

static void
draw_shape(const Camera &camera, PixelBuffer b, Shape s, v2 *vertex_buffer, v2 pos, unsigned int color) {
    for (int i = 0; i < s.index_count; i += 3) {
        draw_triangle(camera, b, s.i + i, vertex_buffer, pos, color);
    }
}

static void
draw_entities(GameState *g, VertexBuffer *vb, PixelBuffer b) {
    static constexpr int MASK = CM_Shape | CM_Pos | CM_Color;
    for (int i = 0; i < g->entity_count; i++) {
        if ((g->entities[i] & MASK) == MASK)
            draw_shape(g->camera, b, g->shapes[i], vb->vertices, g->positions[i], g->colors[i]);
    }
}

static unsigned int
add_entity(GameState *g, unsigned int mask) {
    g->entities[g->entity_count] = mask;
    return g->entity_count++;
}

static void
update_components(GameState *g, unsigned int entity, int mask, void *data) {
    if (mask & CM_Pos) {
        g->positions[entity] = (v2)(*(v2 *)data);
        data = (v2 *)data + 1;
    }
    if (mask & CM_Shape) {
        g->shapes[entity] = (Shape)(*(Shape *)data);
        data = (Shape *)data + 1;
    }
    if (mask & CM_Color) {
        g->colors[entity] = (unsigned int)(*(unsigned int *)data);
        data = (unsigned int *)data + 1;
    }
    if (mask & CM_Velocity) {
        g->velocities[entity] = (v2)(*(v2 *)data);
        data = (v2 *)data + 1;
    }
    if (mask & CM_Actuator) {
        g->actuators[entity] = (Actuator)(*(Actuator *)data);
        data = (Actuator *)data + 1;
    }
    if (mask & CM_Collision) {
        g->collisions[entity] = (Collision)(*(Collision *)data);
        data = (Collision *)data + 1;
    }
}

static void
update_positions(GameState *g, double d_t) {
    static constexpr int MASK = (CM_Pos | CM_Velocity);
    for (int i = 0; i < g->entity_count; i++) {
        if ((g->entities[i] & MASK) == MASK)
            g->positions[i] += g->velocities[i] * d_t;
    }
}

Collision
check_collision(Shape a, Shape b, v2 pos_a, v2 pos_b, v2 vel_a, v2 vel_b, double d_t) {
    // TODO: Find a better solution. Should I allocate memory for this?
    Collision result = {};
    constexpr int MAX_LINES = 64;
    v2 normals[MAX_LINES];
    int normal_count = a.index_count + b.index_count;
    assert(!(a.index_count > MAX_LINES || b.index_count > MAX_LINES));

    for (unsigned int i = 0; i < a.index_count - 1; i++) {
        normals[i] = lnormal(a.v_buffer[a.i[i]], a.v_buffer[a.i[i + 1]]);
    }
    normals[a.index_count - 1] = lnormal(a.v_buffer[a.i[a.index_count - 1]], a.v_buffer[a.i[0]]);
    for (unsigned int i = 0; i < b.index_count - 1; i++) {
        normals[i + a.index_count] = lnormal(b.v_buffer[b.i[i]], b.v_buffer[b.i[i + 1]]);
    }
    normals[a.index_count + b.index_count - 1] = lnormal(b.v_buffer[b.i[b.index_count - 1]], b.v_buffer[b.i[0]]);

    float min_overlap = FLT_MAX;
    v2 min_overlap_axis;

    bool collides = true;
    for (int i = 0; i < a.index_count + b.index_count; i++) {
        float min_a = FLT_MAX;
        float max_a = -FLT_MAX;
        float min_b = FLT_MAX;
        float max_b = -FLT_MAX;
        for (int a_i = 0; a_i < a.index_count; a_i++) {
            float x = dot(a.v_buffer[a.i[a_i]] + pos_a, normals[i]);
            float x_extended = dot(a.v_buffer[a.i[a_i]] + pos_a - vel_a * d_t, normals[i]);
            min_a = min(min(x, min_a), x_extended);
            max_a = max(max(x, max_a), x_extended);
        }
        for (int b_i = 0; b_i < b.index_count; b_i++) {
            float x = dot(b.v_buffer[b.i[b_i]] + pos_b, normals[i]);
            float x_extended = dot(b.v_buffer[b.i[b_i]] + pos_b - vel_b * d_t, normals[i]);
            min_b = min(min(x, min_b), x_extended);
            max_b = max(max(x, max_b), x_extended);
        }
        if (min_a > max_b || max_a < min_b)
            return result; // Does not collide
        float overlap = min(max_a - min_b, max_b - min_a);
        float o1 = max_a - min_b;
        float o2 = max_b - min_a;
        float o = min(o1, o2);
        if (overlap < min_overlap) {
            min_overlap = overlap;
            min_overlap_axis = normals[i];
            if (o1 < o2)
                min_overlap_axis *= -1;                
        }
    }
    result.collided = true;
    assert(min_overlap > (-EPSILON));
    result.mtv = normalize(min_overlap_axis) * min_overlap;
    if (magnitude(result.mtv) > 1)
        int break_here = true;
    return result;
}

static void
resolve_collision(Collision col, v2 *pos_a, v2 *pos_b, v2 *vel_a, v2 *vel_b) {
    *vel_a -= projection(*vel_a, col.mtv);
    *pos_a += (1 + EPSILON) * col.mtv;
    // TODO: Handle response of other object
}

static void
resolve_collision(Collision col, v2 *pos_a, v2 *pos_b, v2 *vel_a) {
    *vel_a -= projection(*vel_a, col.mtv);
    // TODO: This only works for collisions in the positive directions!
    *pos_a += (1 + EPSILON) * col.mtv;
}

static void
handle_collisions(GameState *g, double d_t) {
    static constexpr int MASK = CM_Pos | CM_Collision;
    for (unsigned int i = 0; i < g->entity_count; i++) {
        if ((g->entities[i] & (MASK | CM_Velocity)) != (MASK | CM_Velocity))
            continue;
        for (unsigned int j = 0; j < g->entity_count; j++) {
            if (i == j || (g->entities[j] & MASK) != MASK)
                continue;
            Collision col = check_collision(g->shapes[i], g->shapes[j],
                    g->positions[i], g->positions[j], g->velocities[i],
                    g->velocities[j], d_t); if (col.collided) {
                if (g->entities[j] & CM_Velocity) {
                    resolve_collision(col, &g->positions[i], &g->positions[j],
                            &g->velocities[i], &g->velocities[j]);
                } else {
                    resolve_collision(col, &g->positions[i], &g->positions[j],
                            &g->velocities[i]);
                }
            }
        }
    }
}

GameMemory
initializeGame(unsigned long long mem_size) {

    // Allocate game memory.
    GameMemory mem = {};
    mem.size = mem_size;
    mem.data = malloc(mem_size);
    if (!mem.data) {
        printf("Could not allocate game memory. Quitting...\n");
        exit(1);
    }
    mem.free = mem.data;

    // Initialize memory layout.
    MemoryLayout *ml = (MemoryLayout *)get_memory(&mem, sizeof(GameState));
    GameState *gs = (GameState *)get_memory(&mem, sizeof(GameState));
    VertexBuffer *vb = (VertexBuffer *)get_memory(&mem, sizeof(VertexBuffer));
    IndexBuffer *ib = (IndexBuffer *)get_memory(&mem, sizeof(IndexBuffer));
    *ml = {.gs = gs, .vb = vb, .ib = ib};

    // Initialize vertex and index buffer.
    static constexpr unsigned int MAX_V = 64;
    vb->vertices = (v2 *)get_memory(&mem, MAX_V * sizeof(v2));
    vb->max_count = MAX_V;
    vb->count = 0;
    static constexpr unsigned int MAX_I = 64;
    ib->indices = (unsigned int *)get_memory(&mem, MAX_I * sizeof(unsigned int));
    ib->max_count = MAX_I;
    ib->count = 0;

    // Initialize entity components.
    gs->max_entity_count = 64;
    gs->entities = (unsigned int *)get_memory(&mem, gs->max_entity_count * sizeof(unsigned int));
    gs->positions = (v2 *)get_memory(&mem, gs->max_entity_count * sizeof(v2));
    gs->shapes = (Shape *)get_memory(&mem, gs->max_entity_count * sizeof(Shape));
    gs->colors = (unsigned int *)get_memory(&mem, gs->max_entity_count * sizeof(unsigned int));
    gs->velocities = (v2 *)get_memory(&mem, gs->max_entity_count * sizeof(v2));
    gs->actuators = (Actuator *)get_memory(&mem, gs->max_entity_count * sizeof(Actuator));
    gs->collisions = (Collision *)get_memory(&mem, gs->max_entity_count * sizeof(Collision));

    // Initialize camera.
    gs->camera.pos = {0.0f, 0.0f};
    gs->camera.scale = 40.0f;
    gs->camera.width = WIN_WIDTH;
    gs->camera.height = WIN_HEIGHT;

    // Initialize player.
    constexpr unsigned int count_v = 3;
    v2 vertices[count_v] = {
        { 0.0, -0.5},
        { 0.5,  0.5},
        {-0.5,  0.5}
    };
    constexpr unsigned int count_i = 3;
    unsigned int indices[count_i] = {
        0, 1, 2
    };
#if 0
    constexpr unsigned int count_v = 8;
    v2 vertices[count_v] = {
        {-0.5, -0.5},
        { 0.5, -0.5},
        { 0.5,  0.5},
        {-0.5,  0.5},
        { 0.0, -2.5},
        { 2.5,  0.0},
        { 0.0,  2.5},
        {-2.5,  0.0}
    };
    constexpr unsigned int count_i = 18;
    unsigned int indices[count_i] = {
        0, 1, 2,
        0, 2, 3,
        4, 1, 0,
        5, 2, 1,
        6, 3, 2,
        7, 0, 3
    };
#endif
    int player_mask = CM_Pos | CM_Shape | CM_Color | CM_Velocity | CM_Actuator | CM_Collision;
    gs->player_index = add_entity(gs, player_mask);
    Player player = {
        .pos = {0, 0},
        .shape = create_polygon(vb, ib, vertices, count_v, indices, count_i),
        .color = 0xFFFFCC55,
        .velocity = {0, 0},
        .actuator = {40, 3},
        .collision = {0}
    };
    update_components(gs, gs->player_index, player_mask, (void *)&player);

    int scenery_mask = CM_Pos | CM_Shape | CM_Color | CM_Collision;
    unsigned int wall = add_entity(gs, scenery_mask);
    Scenery wall_data = {
        .pos = {1, 3},
        .shape = create_polygon(vb, ib, vertices, count_v, indices, count_i),
        .color = 0xFF55CCFF,
        .collision = {0}
    };
    update_components(gs, wall, scenery_mask, (void *)&wall_data);

    return mem;
}

void
gameUpdateAndRender(double d_t, GameMemory mem, GameInput *input, PixelBuffer buffer) {
    MemoryLayout *ml = (MemoryLayout *)mem.data;
    GameState *game_state = ml->gs;
    VertexBuffer *vb = ml->vb;

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
    int p = game_state->player_index;

    // PHYSICS
    accelerate(dir, 1.0f, game_state->actuators[p], &game_state->velocities[p], d_t);
    update_positions(game_state, d_t);

    handle_collisions(game_state, d_t);

    // RENDER
    clear_pixel_buffer(buffer, 0);
    draw_entities(game_state, vb, buffer);

    if (d_t > 1.0f/(FRAME_RATE-1))
        printf("Frame rate drop: %.2f FPS\n", 1/d_t);
}

