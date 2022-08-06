#include <stdio.h>
#include <unistd.h>
#include <cstdlib>

#include "handmade.h"
#include "math_handmade.h"

struct SceneryObject {
    v2 pos;
    void *shape;
};

struct PhysicsObject {
    bool collides;
    v2 pos;
    v2 velocity;
    float friction;
};

struct Player {
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

struct GameState {
    Player player;
    Rectangle npc;
    Camera camera;
    Triangle triangle; // Testing
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
world_to_pixel_space(v2 coord, const Camera &camera);

static Rectangle
world_to_pixel_space(Rectangle rect, const Camera &camera);

static Triangle
world_to_pixel_space(Triangle t, const Camera &camera);

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
world_to_pixel_space(v2 coord, const Camera &camera) {
    v2 offset = {camera.width / 2.0f, camera.height / 2.0f};
    return (coord - camera.pos) * camera.scale + offset;
}

static Rectangle
world_to_pixel_space(Rectangle rect, const Camera &camera) {
    return Rectangle(world_to_pixel_space(rect.min, camera), world_to_pixel_space(rect.max, camera));
}

static Triangle
world_to_pixel_space(Triangle t, const Camera &camera) {
    return Triangle{world_to_pixel_space(t.a, camera), world_to_pixel_space(t.b, camera), world_to_pixel_space(t.c, camera)};
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
    rect = world_to_pixel_space(rect, camera);
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
    Triangle tp = world_to_pixel_space(t, camera);
    Rectangle screen = Rectangle({0, 0}, v2{(float)camera.width, (float)camera.height});
    Rectangle draw_area = intersection(boundingBox(tp), screen);
    if (isPositive(draw_area)) {
        int pitch = buffer.width * buffer.bytes_per_pixel;
        for (int y = draw_area.min.y; y < draw_area.max.y; y++) {
            unsigned int *row = (unsigned int *)(buffer.data + y * pitch);
            for (int x = draw_area.min.x; x < draw_area.max.x; x++) {
                unsigned int *p = row + x;
                if (isInsideTriangle(v2{(float)x, (float)y}, tp))
                    *p = color;
            }
        }
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

#define POLYGON_TEST
#ifdef POLYGON_TEST

    game_state->triangle = Triangle{v2{0, -1}, v2{1, 1}, v2{-1, 1}};

#else
    game_state->player.physics = {};
    game_state->player.physics.pos = {0.0f, 0.0f};
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

    drawTriangle(buffer, game_state->triangle, game_state->camera, 0xFF00FFFF);
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
    game_state->player.physics.pos += game_state->player.physics.velocity * d_t;
    PhysicsCollision c = collision_detection(game_state->player.shape + game_state->player.physics.pos, game_state->npc, game_state->player.physics.velocity, d_t);
    collision_resolution(c, &game_state->player.physics.pos, 0, &game_state->player.physics.velocity, 0, d_t);
    // RENDER
    clearScreen(buffer, 0);

    drawRectangle(buffer, minkowski_sum(game_state->player.shape + game_state->player.physics.pos, game_state->npc).rect, game_state->camera, 0xFF0000AA);
    unsigned int npc_color = 0xFF0000FF;
    if (collision_detection(game_state->player.shape + game_state->player.physics.pos, game_state->npc, game_state->player.physics.velocity, d_t).collided) {
        npc_color = 0xFF00FF00;
    }
    drawRectangle(buffer, game_state->npc, game_state->camera, npc_color);

    drawRectangle(buffer, game_state->player.shape + game_state->player.physics.pos, game_state->camera, game_state->player.color);
#endif



#if 0
    if (d_t > 1.0f/(FRAME_RATE-1))
        printf("Frame rate drop: %.2f FPS\n", 1/d_t);
#endif
}

