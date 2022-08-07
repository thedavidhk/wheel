#include <stdio.h>
#include <float.h>
#include <string.h>

#include "handmade.h"
#include "math_handmade.h"

typedef enum {
    CM_Pos =            1 << 0,
    CM_PhysicsObject =  1 << 1,
    CM_Shape =          1 << 2,
    CM_Acceleration =   1 << 3,
    CM_Color =          1 << 4
} ComponentMask; 

typedef enum {
    ST_Rectangle,
    ST_TRIANGLE,
    ST_POLYGON
} ShapeType;

struct PhysicsObject {
    bool collides;
    v2 velocity;
    float friction;
};

struct Player {
    v2 pos;
    PhysicsObject physics;
    Rectangle shape;
    float speed;
    unsigned int color;
};

struct Npc {
    Rectangle shape;
    v2 pos;
    unsigned int color;
};

struct Scenery {
    Rectangle shape;
    v2 pos;
    unsigned int color;
};

struct Camera {
    v2 pos;
    float scale; // pixels per meter
    unsigned int width, height;
};

struct Shape {
    unsigned int *i;
    unsigned int index_count;
};

struct GameState {
    Player player;
    Rectangle npc;
    Camera camera;
    Polygon triangle; // Testing

    unsigned int vertex_count;
    unsigned int max_vertex_count;
    v2 *vertex_buffer;
    unsigned int index_count;
    unsigned int max_index_count;
    unsigned int *index_buffer;

    unsigned int player_index;

    Shape testshape;

    // Entities - Testing!
    unsigned int entity_count;
    unsigned int max_entity_count;
    unsigned int *entities;
    v2 *positions;
    Shape *shapes;
    PhysicsObject *physics_objects;
    float *accelerations;
    unsigned int *colors;
};

struct PhysicsCollision {
    bool collided = false;
    v2 intersection;
    v2 normal;
};

static void
clearScreen(PixelBuffer buffer, unsigned int color);

static void
accelerate(PhysicsObject *entity, v2 direction, float force, double d_t);

static void
apply_friction(PhysicsObject *entity, double d_t);

static void
apply_friction(PhysicsObject *entity, double d_t);

static v2
world_to_screen_space(v2 coord, const Camera &camera);

static Rectangle
world_to_screen_space(Rectangle rect, const Camera &camera);

static Triangle
world_to_screen_space(Triangle t, const Camera &camera);

static PhysicsCollision
collision_detection(Rectangle a, v2 b, v2 vel_a, double d_t);

static PhysicsCollision
collision_detection(Rectangle a, Rectangle b, v2 vel_a, double d_t);

static void
collision_resolution(PhysicsCollision collision, v2 *pos_a, v2 *pos_b, v2 *vel_a, v2 *vel_b, double d_t);

static void
drawRectangle(PixelBuffer buffer, Rectangle rect, Camera camera, unsigned int color);

static void
drawTriangle(PixelBuffer buffer, Triangle t, Camera camera, unsigned int color);

static void
clearScreen(PixelBuffer buffer, unsigned int color) {
    for (unsigned int i = 0; i < buffer.width * buffer.height * sizeof(color); i ++) {
        buffer.data[i] = color;
    }
}

static void
accelerate(PhysicsObject *entity, v2 direction, float force, double d_t) {
    float mag = magnitude(direction);
    v2 acceleration = {0};
    if (mag > EPSILON) {
        direction /= mag;
    }
    acceleration = direction * force;
    entity->velocity += acceleration * d_t;
}

static void
apply_friction(PhysicsObject *entity, double d_t) {
    accelerate(entity, -entity->velocity, magnitude(entity->velocity) * entity->friction, d_t);
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

// TODO: Handle velocity of other object
static PhysicsCollision
collision_detection(Rectangle a, v2 b, v2 vel_b, double d_t) {
    PhysicsCollision c = {0};
    bool collided = (b.x > a.min.x && b.x < a.max.x) && (b.y > a.min.y && b.y < a.max.y);
    if (!collided)
        return c;
    Line side[4] = {};
    get_sides(a, side);
    Intersection intersect = {};
    for (int i = 0; i < 4; i++) {
        Intersection new_intersect = intersection(Line{b, b - vel_b * d_t}, side[i]);
        if (!new_intersect.exists)
            continue;
        if (!intersect.exists || (intersect.exists &&
                    magnitude(intersect.coordinates - b) <
                    magnitude(new_intersect.coordinates - b))) {
            intersect = new_intersect;
            c.normal = rnormal(side[i]);
            c.intersection = intersect.coordinates;
            c.collided = true;
        }
    }
    assert(intersect.exists);
    return c;
}

static PhysicsCollision
collision_detection(Rectangle a, Rectangle b, v2 a_vel, double d_t) {
    MinkowskiSum m = minkowski_sum(a, b);
    PhysicsCollision c = collision_detection(m.rect, m.point, a_vel, d_t);
    return c;
}

// TODO: Handle object b
static void
collision_resolution(PhysicsCollision c, v2 *pos_a, v2 *pos_b, v2 *vel_a, v2 *vel_b, double d_t) {
    if (!c.collided)
        return;
    v2 rest_old = *pos_a - c.intersection;
    v2 normal_unit = c.normal / magnitude(c.normal);
    v2 rest_new = rest_old - dot(rest_old, normal_unit) * normal_unit;
    *pos_a = c.intersection + rest_new;
    *vel_a -= dot(*vel_a, normal_unit) * normal_unit;
}

static void
drawRectangle(PixelBuffer buffer, Rectangle rect, Camera camera, unsigned int color) {
    rect = world_to_screen_space(rect, camera);
    Rectangle screen = Rectangle({0, 0}, {(float)camera.width, (float)camera.height});
    Rectangle pixels = intersection(rect, screen);
    if (isPositive(pixels)) {
        int pitch = buffer.width * buffer.bytes_per_pixel;
        for (int y = pixels.min.y; y < pixels.max.y; y++) {
            unsigned int *row = (unsigned int *)(buffer.data + y * pitch);
            for (int x = pixels.min.x; x < pixels.max.x; x++) {
                unsigned int *p = row + x;
                *p = color;
            }
        }
    }
}

static void
drawTriangle(PixelBuffer buffer, Triangle t, Camera camera, unsigned int color) {
    Triangle tp = world_to_screen_space(t, camera);
    Rectangle screen = Rectangle({0, 0}, v2{(float)camera.width, (float)camera.height});
    Rectangle draw_area = intersection(boundingBox(tp), screen);
    if (isPositive(draw_area)) {
        int pitch = buffer.width * buffer.bytes_per_pixel;
        for (int y = draw_area.min.y; y < draw_area.max.y; y++) {
            unsigned int *row = (unsigned int *)(buffer.data + y * pitch);
            bool drawn = false;
            for (int x = draw_area.min.x; x < draw_area.max.x; x++) {
                unsigned int *p = row + x;
                if (isInsideTriangle(v2{(float)x, (float)y}, tp)) {
                    *p = color;
                    drawn = true;
                } else if (drawn) {
                    break; // If row has been drawn, skip to next row
                }
            }
        }
    }
}

static void
drawPolygon(PixelBuffer buffer, Polygon p, Camera camera, unsigned int color) {
    for (int i = 2; i < p.vertex_count; i++) {
        Triangle t = Triangle{p.vertices[0], p.vertices[i - 1], p.vertices[i]};
        drawTriangle(buffer, t, camera, color);
    }
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
create_polygon(GameState *g, const v2 *verts, unsigned int count_v, const unsigned int *indices, unsigned int count_i) {
    Shape p = {};
    p.index_count = count_i;
    memcpy(g->vertex_buffer + g->vertex_count, verts, count_v * sizeof(*verts));
    g->vertex_count += count_v;
    memcpy(g->index_buffer + g->index_count, indices, count_i * sizeof(*indices));
    p.i = g->index_buffer + g->index_count;
    g->index_count += count_i;
    return p;
};

static Shape
create_rectangle(GameState *g, v2 min, v2 max) {
    v2 verts[4] = {min, v2{max.x, min.y}, max, v2{min.x, max.y}};
    unsigned int indices[6] = {0, 1, 2, 0, 2, 3};
    return create_polygon(g, verts, 4, indices, 6);
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

// TODO: only take camera and index/vertex pointers instead of game state
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

    Rectangle draw_area = bounding_box(tri, 3);
    int pitch = b.width * b.bytes_per_pixel;
    for (int y = draw_area.min.y; y < draw_area.max.y; y++) {
        unsigned int *row = (unsigned int *)(b.data + y * pitch);
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
draw_entity(GameState *g, unsigned int index, PixelBuffer b) {
    draw_shape(g->camera, b, g->shapes[index], g->vertex_buffer, g->positions[index], g->colors[index]);
}

static void
draw_entities(GameState *g, PixelBuffer b) {
    for (int i = 0; i < g->entity_count; i++) {
        if ((g->entities[i] & CM_Shape) && (g->entities[i] & CM_Pos))
            draw_entity(g, i, b);
    }
}

static unsigned int
add_entity(GameState *g, unsigned int mask) {
    g->entities[g->entity_count] = mask;
    return g->entity_count++;
}

struct TestPlayer {
    v2 pos;
    PhysicsObject physics;
    Shape shape;
    float acceleration;
    unsigned int color;
};

//update_component(player_index, player_mask, (void *)&player);
static void
update_components(GameState *g, unsigned int entity, int mask, void *data) {
    if (mask & CM_Pos) {
        g->positions[entity] = (v2)(*(v2 *)data);
        data = (v2 *)data + 1;
    }
    if (mask & CM_PhysicsObject) {
        g->physics_objects[entity] = (PhysicsObject)(*(PhysicsObject *)data);
        data = (PhysicsObject *)data + 1;
    }
    if (mask & CM_Shape) {
        g->shapes[entity] = (Shape)(*(Shape *)data);
        data = (Shape *)data + 1;
    }
    if (mask & CM_Acceleration) {
        g->accelerations[entity] = (float)(*(float *)data);
        data = (float *)data + 1;
    }
    if (mask & CM_Color) {
        g->colors[entity] = (unsigned int)(*(unsigned int *)data);
        data = (unsigned int *)data + 1;
    }
}


GameMemory
initializeGame(unsigned long long mem_size) {
    GameMemory mem = {};
    mem.size = mem_size;
    mem.data = malloc(mem_size);
    if (!mem.data) {
        printf("Could not allocate game memory. Quitting...\n");
        exit(1);
    }

    GameState *game_state = (GameState *)mem.data;
    mem.free = (char *)mem.data + sizeof(game_state);

    game_state->camera.pos = {0.0f, 0.0f};
    game_state->camera.scale = 40.0f;
    game_state->camera.width = WIN_WIDTH;
    game_state->camera.height = WIN_HEIGHT;

#define ENTITY_TEST
#ifdef ENTITY_TEST

    game_state->max_vertex_count = 64;
    game_state->vertex_buffer = (v2 *)get_memory(&mem, game_state->max_vertex_count * sizeof(v2));
    game_state->max_index_count = 64;
    game_state->index_buffer = (unsigned int *)get_memory(&mem, game_state->max_index_count * sizeof(unsigned int));

    // Allocate memory for entities
    game_state->max_entity_count = 64;
    game_state->entities = (unsigned int *)get_memory(&mem, game_state->max_entity_count * sizeof(unsigned int));
    game_state->positions = (v2 *)get_memory(&mem, game_state->max_entity_count * sizeof(v2));
    game_state->shapes = (Shape *)get_memory(&mem, game_state->max_entity_count * sizeof(Shape));
    game_state->physics_objects = (PhysicsObject *)get_memory(&mem, game_state->max_entity_count * sizeof(PhysicsObject));
    game_state->accelerations = (float *)get_memory(&mem, game_state->max_entity_count * sizeof(float));
    game_state->colors = (unsigned int *)get_memory(&mem, game_state->max_entity_count * sizeof(unsigned int));

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

    int player_mask = CM_Pos | CM_PhysicsObject | CM_Shape | CM_Acceleration | CM_Color;
    game_state->player_index = add_entity(game_state, player_mask);
    TestPlayer player = {
        .pos = {0, 0},
        .physics = {true, v2{0, 0}, 7.0f},
        .shape = create_polygon(game_state, vertices, count_v, indices, count_i),
        .acceleration = 100,
        .color = 0xFFFFCC55
    };

    update_components(game_state, game_state->player_index, player_mask, (void *)&player);

#else
    game_state->player.physics = {};
    game_state->player.pos = {0.0f, 0.0f};
    game_state->player.physics.friction = 7.0f;
    game_state->player.color = 0xFFFF0000;
    game_state->player.speed = 100.0f;
    game_state->player.shape = Rectangle({-0.5, -0.5}, {0.5, 0.5});

    game_state->npc.min = {2.0f, -1.0f};
    game_state->npc.max = {4.0f, 3.0f};
#endif

    return mem;
}

void
gameUpdateAndRender(double d_t, GameMemory mem, GameInput *input, PixelBuffer buffer) {
    GameState *game_state = (GameState *)mem.data;

#ifdef POLYGON_TEST
    clearScreen(buffer, 0);

    drawPolygon(buffer, game_state->triangle, game_state->camera, 0xFF00FFFF);

#elif defined(ENTITY_TEST)


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

    accelerate(&game_state->physics_objects[game_state->player_index], dir, game_state->accelerations[game_state->player_index], d_t);
    apply_friction(&game_state->physics_objects[game_state->player_index], d_t);
    game_state->positions[game_state->player_index] += game_state->physics_objects[game_state->player_index].velocity * d_t;

    clearScreen(buffer, 0);
    draw_entities(game_state, buffer);

    
#else

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

    accelerate(&game_state->player.physics, dir, game_state->player.speed, d_t);

    // PHYSICS
    apply_friction(&game_state->player.physics, d_t);
    game_state->player.pos += game_state->player.physics.velocity * d_t;
    PhysicsCollision c = collision_detection(game_state->player.shape + game_state->player.pos, game_state->npc, game_state->player.physics.velocity, d_t);
    collision_resolution(c, &game_state->player.pos, 0, &game_state->player.physics.velocity, 0, d_t);
    // RENDER
    clearScreen(buffer, 0);

    drawRectangle(buffer, minkowski_sum(game_state->player.shape + game_state->player.pos, game_state->npc).rect, game_state->camera, 0xFF0000AA);
    unsigned int npc_color = 0xFF0000FF;
    if (collision_detection(game_state->player.shape + game_state->player.pos, game_state->npc, game_state->player.physics.velocity, d_t).collided) {
        npc_color = 0xFF00FF00;
    }
    drawRectangle(buffer, game_state->npc, game_state->camera, npc_color);

    drawRectangle(buffer, game_state->player.shape + game_state->player.pos, game_state->camera, game_state->player.color);
#endif



#if 0
    if (d_t > 1.0f/(FRAME_RATE-1))
        printf("Frame rate drop: %.2f FPS\n", 1/d_t);
#endif
}

