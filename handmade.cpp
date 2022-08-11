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
    CM_Force            = 1 << 4,
    CM_Mass             = 1 << 5,
    CM_Friction         = 1 << 6,
    CM_Collision        = 1 << 7,
    CM_BoxCollider      = 1 << 8
} ComponentMask; 

struct VertexBuffer {
    v2 *vertices;
    unsigned int count, max_count;
};

struct IndexBuffer {
    unsigned int *indices;
    unsigned int count, max_count;
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

struct Mass {
    v2 com;
    float value;
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
    float max_force;
    Mass mass;
    float friction;
    Collision collision;
};

struct Moveable {
    v2 pos;
    Shape shape;
    unsigned int color;
    v2 velocity;
    Mass mass;
    float friction;
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
    float *forces;
    Mass *masses;
    float *frictions;
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
accelerate(v2 direction, float force, float mass, float friction, v2 *velocity, double d_t);

static v2
world_to_screen_space(v2 coord, const Camera &camera);

static void*
get_memory(GameMemory *mem, unsigned long long size);

static Shape
create_polygon(VertexBuffer *vb, IndexBuffer *ib, const v2 *verts, unsigned int
        count_v, const unsigned int *indices, unsigned int count_i);

static Shape
create_rectangle(VertexBuffer *vb, IndexBuffer *ib, v2 min, v2 max);

static void
draw_triangle(const Camera &camera, PixelBuffer b, v2 p0, v2 p1, v2 p2, unsigned int color);

static void
draw_shape(const Camera &camera, PixelBuffer b, Shape s, v2 *vertex_buffer, v2 pos, unsigned int color);

static void
draw_entities(GameState *g, VertexBuffer *vb, PixelBuffer b);

static unsigned int
add_entity(GameState *g, unsigned int mask);

static void
update_components(GameState *g, unsigned int entity, int mask, void *data);

static void
update_velocities(GameState *g, double d_t);

static void
update_positions(GameState *g, double d_t);

static Collision
check_collision(Shape a, Shape b, v2 pos_a, v2 pos_b, v2 vel_a, v2 vel_b, double d_t);

static void
handle_collisions(GameState *g, double d_t);

static void
resolve_collision(Collision col, v2 *pos_a, v2 *pos_b, v2 *vel_a, v2 *vel_b, float m_a, float m_b);

static void
resolve_collision(Collision col, v2 *pos_a, v2 *pos_b, v2 *vel_a);

static void
clear_pixel_buffer(PixelBuffer b, unsigned int color) {
    for (int i = 0; i < b.width * b.height; i ++) {
        b.data[i] = color;
    }
}

static void
accelerate(v2 direction, float force, float mass, float friction, v2 *velocity, double d_t) {
    direction = normalize(direction);
    *velocity += (direction * force/mass) * d_t;
}

static v2
world_to_screen_space(v2 coord, const Camera &camera) {
    v2 offset = {camera.width / 2.0f, camera.height / 2.0f};
    return (coord - camera.pos) * camera.scale + offset;
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
    p.i = ib->indices + ib->count;
    memcpy(vb->vertices + vb->count, verts, count_v * sizeof(*verts));
    memcpy(p.i, indices, count_i * sizeof(*indices));
    for (unsigned int i = 0; i < count_i; i++) {
        p.i[i] += vb->count;
    }
    vb->count += count_v;
    ib->count += count_i;
    return p;
};

static Shape
create_rectangle(VertexBuffer *vb, IndexBuffer *ib, v2 min, v2 max) {
    v2 verts[4] = {min, v2{max.x, min.y}, max, v2{min.x, max.y}};
    unsigned int indices[6] = {0, 1, 2, 0, 2, 3};
    return create_polygon(vb, ib, verts, 4, indices, 6);
};

static void
debug_draw_point(PixelBuffer b, v2 p, float radius, unsigned int color) {
    for (int y = max(0, p.y - radius); y <= min(b.height - 1, p.y + radius); y++) {
        for (int x = max(0, p.x - radius); x <= min(b.height - 1, p.x + radius); x++) {
            if (magnitude(v2{(float)x, (float)y} - p) <= radius)
                b.data[x + y * b.width] = color;
        }
    }
}

static void
draw_triangle(const Camera &camera, PixelBuffer b, v2 p0, v2 p1, v2 p2, unsigned int color) {
    v2 tri[3];
    tri[0] = world_to_screen_space(p0, camera);
    tri[1] = world_to_screen_space(p1, camera);
    tri[2] = world_to_screen_space(p2, camera);
    int top;
    if (tri[0].y <= tri[1].y + EPSILON)
        if (tri[0].y <= tri[2].y + EPSILON)
            top = 0;
        else
            top = 2;
    else
        top = 1;
    v2 p[3];
    p[0] = tri[top];
    p[1] = tri[(top + 1) % 3];
    p[2] = tri[(top + 2) % 3];

    //debug_draw_point(b, p[0], 5, 0xFFFF0000);
    //debug_draw_point(b, p[1], 5, 0xFF00FF00);
    //debug_draw_point(b, p[2], 5, 0xFF0000FF);

    float dx_start_high = (p[0].x - p[2].x) / (p[0].y - p[2].y);
    float dx_start_low = (p[1].x - p[2].x) / (p[1].y - p[2].y);
    float dx_end_high = (p[1].x - p[0].x) / (p[1].y - p[0].y);
    float dx_end_low = (p[1].x - p[2].x) / (p[1].y - p[2].y);
    float x_start = p[0].x;
    float x_end = x_start;
    if (p[0].y >= p[1].y - EPSILON)
        x_end = p[1].x;
    float y_start = p[0].y;
    float y_end = min(b.height - 1, (int) max(p[1].y, p[2].y));

    for (int y = y_start; y <= y_end; y++) {
        if (y >= 0) {
            for (int x = max(0, (int)x_start); x < min(b.width, (int)x_end); x++) {
                b.data[x + y * (b.width)] = color;
            }
        }
        if (y < p[1].y - 1 + EPSILON)
            x_end += dx_end_high;
        else
            x_end += dx_end_low;
        if (y < p[2].y - 1 + EPSILON)
            x_start += dx_start_high;
        else
            x_start += dx_start_low;
    }
}

static void
draw_shape(const Camera &camera, PixelBuffer b, Shape s, v2 *vertex_buffer, v2 pos, unsigned int color) {
    for (unsigned int i = 0; i < s.index_count; i += 3) {
        //draw_triangle(camera, b, s.i + i, vertex_buffer, pos, color);
        v2 p0 = vertex_buffer[s.i[i]] + pos;
        v2 p1 = vertex_buffer[s.i[i + 1]] + pos;
        v2 p2 = vertex_buffer[s.i[i + 2]] + pos;
        draw_triangle(camera, b, p0, p1, p2, color);
    }
}

static void
draw_entities(GameState *g, VertexBuffer *vb, PixelBuffer b) {
    static constexpr int MASK = CM_Shape | CM_Pos | CM_Color;
    for (unsigned int i = 0; i < g->entity_count; i++) {
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
    if (mask & CM_Force) {
        g->forces[entity] = (float)(*(float *)data);
        data = (float *)data + 1;
    }
    if (mask & CM_Mass) {
        g->masses[entity] = (Mass)(*(Mass *)data);
        data = (Mass *)data + 1;
    }
    if (mask & CM_Friction) {
        g->frictions[entity] = (float)(*(float *)data);
        data = (float *)data + 1;
    }
    if (mask & CM_Collision) {
        g->collisions[entity] = (Collision)(*(Collision *)data);
        data = (Collision *)data + 1;
    }
}

static void
update_velocities(GameState *g, double d_t) {
    static constexpr int MASK = (CM_Pos | CM_Velocity);
    for (unsigned int i = 0; i < g->entity_count; i++) {
        if ((g->entities[i] & MASK) == MASK) {
            if (g->entities[i] & CM_Mass)
                g->velocities[i] += v2{0, 1} * 9.87 * d_t;
            if (g->entities[i] & CM_Friction)
                g->velocities[i] -= g->velocities[i] * g->frictions[i] * d_t;
        }
    }
}

static void
update_positions(GameState *g, double d_t) {
    static constexpr int MASK = (CM_Pos | CM_Velocity);
    for (unsigned int i = 0; i < g->entity_count; i++) {
        if ((g->entities[i] & MASK) == MASK) {
            g->positions[i] += g->velocities[i] * d_t;
        }
    }
}

Collision
check_collision(Shape a, Shape b, v2 pos_a, v2 pos_b, v2 vel_a, v2 vel_b, double d_t) {
    // TODO: Probably allocate memory to allow for a larger number of lines.
    Collision result = {};
    constexpr int MAX_LINES = 64;
    v2 normals[MAX_LINES];
    assert(!(a.index_count > MAX_LINES || b.index_count > MAX_LINES));

    // Get line normals as axes
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

    // Project vertices onto axes
    for (unsigned int i = 0; i < a.index_count + b.index_count; i++) {
        float min_a = FLT_MAX;
        float max_a = -FLT_MAX;
        float min_b = FLT_MAX;
        float max_b = -FLT_MAX;
        normals[i] = normalize(normals[i]);

        for (unsigned int a_i = 0; a_i < a.index_count; a_i++) {
            float x = dot(a.v_buffer[a.i[a_i]] + pos_a, normals[i]);
            // Extend collider to include origin location
            float x_extended = dot(a.v_buffer[a.i[a_i]] + pos_a + vel_a * d_t, normals[i]);
            min_a = min(min(x, min_a), x_extended);
            max_a = max(max(x, max_a), x_extended);
        }
        for (unsigned int b_i = 0; b_i < b.index_count; b_i++) {
            float x = dot(b.v_buffer[b.i[b_i]] + pos_b, normals[i]);
            // Extend collider to include origin location
            float x_extended = dot(b.v_buffer[b.i[b_i]] + pos_b + vel_b * d_t, normals[i]);
            min_b = min(min(x, min_b), x_extended);
            max_b = max(max(x, max_b), x_extended);
        }
        if (min_a >= max_b || max_a <= min_b)
            return result; // Does not collide

        // Determine minimum translation vector
        float o1 = max_a - min_b;
        float o2 = max_b - min_a;
        float o = min(o1, o2);
        if (o < min_overlap) {
            min_overlap = o;
            min_overlap_axis = normals[i];
            if (o1 < o2)
                min_overlap_axis *= -1;                
        }
    }
    result.collided = true;
    assert(min_overlap > (-EPSILON));
    result.mtv = normalize(min_overlap_axis) * min_overlap;
    return result;
}

static void
resolve_collision(Collision col, v2 *pos_a, v2 *pos_b, v2 *vel_a, v2 *vel_b,
                  float m_a, float m_b)
{
    v2 dv_a = projection(*vel_a, col.mtv);
    v2 dv_b = projection(*vel_b, col.mtv);
    v2 dv = m_b * dv_b + m_a * dv_a;
    *vel_a += dv / (m_a + m_b) - dv_a;
    *vel_b += dv / (m_a + m_b) - dv_b;
    *pos_a += col.mtv;
    *pos_b -= col.mtv;
}

static void
resolve_collision(Collision col, v2 *pos_a, v2 *pos_b, v2 *vel_a) {
    *vel_a -= projection(*vel_a, col.mtv);
    *pos_a += col.mtv;
}

static void
handle_collisions(GameState *g, double d_t) {
    static constexpr int MASK = CM_Pos | CM_Collision;
    for (unsigned int i = 0; i < g->entity_count; i++) {
        if ((g->entities[i] & (MASK | CM_Velocity | CM_Mass)) != (MASK | CM_Velocity | CM_Mass))
            continue;
        for (unsigned int j = 0; j < g->entity_count; j++) {
            if (i == j || (g->entities[j] & MASK) != MASK)
                continue;
            Collision col = check_collision(g->shapes[i], g->shapes[j],
                    g->positions[i], g->positions[j], g->velocities[i],
                    g->velocities[j], d_t); if (col.collided) {
                if (g->entities[j] == (g->entities[j] | CM_Velocity | CM_Mass)) {
                    resolve_collision(col, &g->positions[i], &g->positions[j],
                            &g->velocities[i], &g->velocities[j],
                            g->masses[i].value, g->masses[j].value);
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
    gs->forces = (float *)get_memory(&mem, gs->max_entity_count * sizeof(float));
    gs->masses = (Mass *)get_memory(&mem, gs->max_entity_count * sizeof(Mass));
    gs->frictions = (float *)get_memory(&mem, gs->max_entity_count * sizeof(float));
    gs->collisions = (Collision *)get_memory(&mem, gs->max_entity_count * sizeof(Collision));

    // Initialize camera.
    gs->camera.pos = {0.0f, 0.0f};
    gs->camera.scale = 40.0f;
    gs->camera.width = WIN_WIDTH;
    gs->camera.height = WIN_HEIGHT;

    // Initialize player.
    constexpr unsigned int count_v = 3;
    v2 vertices[count_v] = {
        {-0.5, -0.5},
        { 0.5, -0.3},
        {-0.5,  0.5}
    };
    constexpr unsigned int count_i = 3;
    unsigned int indices[count_i] = {
        0, 1, 2
    };

    int player_mask = CM_Pos | CM_Shape | CM_Color | CM_Velocity | CM_Force |
        CM_Mass | CM_Friction | CM_Collision;
    gs->player_index = add_entity(gs, player_mask);
    Player player = {
        .pos = {0, 0},
        .shape = create_polygon(vb, ib, vertices, count_v, indices, count_i),
        .color = 0xFFFFCC55,
        .velocity = {0, 0},
        .max_force = 40,
        .mass = {v2{0,0}, 1},
        .friction = 3,
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

    unsigned int floor = add_entity(gs, scenery_mask);
    Scenery floor_data = {
        .pos = {0, 6},
        .shape = create_rectangle(vb, ib, {-20, -0.5}, {20, 0.5}),
        .color = 0xFFFFFFFF,
        .collision = {0}
    };
    update_components(gs, floor, scenery_mask, (void *)&floor_data);

    int moveable_mask = CM_Pos | CM_Shape | CM_Color | CM_Collision | CM_Velocity | CM_Mass | CM_Friction;
    unsigned int pebble = add_entity(gs, moveable_mask);
    Moveable pebble_data = {
        .pos = {6, -1},
        .shape = create_polygon(vb, ib, vertices, count_v, indices, count_i),
        .color = 0xFF44CC33,
        .velocity = {-10, 0},
        .mass = {v2{0,0}, 1},
        .friction = 1,
        .collision = {0}
    };
    update_components(gs, pebble, moveable_mask, (void *)&pebble_data);
    return mem;
}

void
gameUpdateAndRender(double frame_time, GameMemory mem, GameInput *input, PixelBuffer buffer) {
    MemoryLayout *ml = (MemoryLayout *)mem.data;
    GameState *game_state = ml->gs;
    VertexBuffer *vb = ml->vb;

    // TODO: Find a more elegant solution
    static bool pause = false;

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
    if (input->pause)
        pause = !pause;
    int p = game_state->player_index;

    if (pause)
        return;

    // PHYSICS
    while (frame_time > 0.0) {
        double d_t = min(frame_time, 1.0d / SIM_RATE);
        accelerate(dir, 1.0f * game_state->forces[p], game_state->masses[p].value, game_state->frictions[p], &game_state->velocities[p], d_t);
        update_velocities(game_state, d_t);
        handle_collisions(game_state, d_t);
        update_positions(game_state, d_t);
        frame_time -= d_t;
    }

    // RENDER
    clear_pixel_buffer(buffer, 0);
    draw_entities(game_state, vb, buffer);
}

