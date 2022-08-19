#ifndef SCENE_WHEEL_H

#include "physics_wheel.h"
#include "render_wheel.h"
#include "memory_wheel.h"

typedef enum {
    CM_Pos              = 1 << 0,
    CM_Rotation         = 1 << 1,
    CM_Mesh             = 1 << 2,
    CM_Color            = 1 << 3,
    CM_Velocity         = 1 << 4,
    CM_Force            = 1 << 5,
    CM_Mass             = 1 << 6,
    CM_Friction         = 1 << 7,
    CM_Collision        = 1 << 8,
    CM_BoxCollider      = 1 << 9
} ComponentMask; 

struct Scene {
    Camera camera;
    uint32 player_index;
    v2 player_movement;
    uint32 hovered_entity;
    uint32 selected_entity; // TODO: turn this into a list(?)
    real64 scene_time;
    bool paused;
    Vertexbuffer *vertexbuffer;
    Indexbuffer *indexbuffer;
    // Entities
    uint32 entity_count;
    uint32 max_entity_count;
    uint32 *entities;
    Transform *transforms;
    Movement *movements;
    Mesh *meshes;
    Color *colors;
    real32 *forces;
    Mass *masses;
    real32 *frictions;
    Collision *collisions;
};

void
handle_collisions(Scene *scene, real64 d_t);

void
draw_entities(Scene *scene, Framebuffer fb);

uint32
add_entity(Scene *scene, uint32 mask);

void
update_velocities(Scene *scene, real64 d_t);

void
update_transforms(Scene *scene, real64 d_t);

Scene *
initialize_scene(AppMemory *mem);

uint32
get_entity_from_screen_pos(int32 x, int32 y, Scene *scene);

void
highlight_entity(Scene *scene, Framebuffer fb, uint32 id);

#define SCENE_WHEEL_H
#endif