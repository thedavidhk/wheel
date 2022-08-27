#ifndef SCENE_WHEEL_H

#include "physics_wheel.h"
#include "render_wheel.h"
#include "memory_wheel.h"

struct DebugGrid {
    v4 bg_color;
    v4 primary_color;
    v4 secondary_color;
    v4 accent_color;
    v2 origin;
    v2 scale;
};

#if NEW_PHYSICS_SYSTEM
struct Scene {
    Camera camera;
    DebugGrid grid;
    Physics physics;
    uint32 player_index;
    uint32 floor_index;
    v2 player_movement;
    uint32 hovered_entity;
    uint32 selected_entity; // TODO: turn this into a list(?)
    real64 scene_time;
    bool paused;
    v2 *vertices;
    v2 *normals;
    //Vertexbuffer *vertexbuffer;
    //Indexbuffer *indexbuffer;
    // Collisions
    uint32 collision_count;
    Collision *collisions; // TODO: probably a linked list?
    // Entities
    uint32 entity_count;
    uint32 max_entity_count;
    uint32 *entities;
    //Transform *transforms;
    //Transform *predicted_transforms;
    //Movement *movements;
    //Movement *predicted_movements;
    //Friction *frictions;
    //Material *materials;
    //Mesh *meshes;
    //Texture *textures;
    //v4 *colors;
    //real32 *forces;
    //Mass *masses;
    //Collision *predicted_collisions;
};
#else
struct Scene {
    Camera camera;
    DebugGrid grid;
    uint32 player_index;
    uint32 floor_index;
    v2 player_movement;
    uint32 hovered_entity;
    uint32 selected_entity; // TODO: turn this into a list(?)
    real64 scene_time;
    bool paused;
    Vertexbuffer *vertexbuffer;
    Indexbuffer *indexbuffer;
    // Collisions
    uint32 collision_count;
    Collision *collisions; // TODO: probably a linked list?
    // Entities
    uint32 entity_count;
    uint32 max_entity_count;
    uint32 *entities;
    Transform *transforms;
    Transform *predicted_transforms;
    Movement *movements;
    Movement *predicted_movements;
    //Friction *frictions;
    Material *materials;
    Mesh *meshes;
    Texture *textures;
    v4 *colors;
    real32 *forces;
    Mass *masses;
    Collision *predicted_collisions;
};
#endif

void
handle_collisions(Scene *scene, real64 d_t);

void
draw_scene(Scene *scene, Framebuffer fb);

void
draw_entities(Scene *scene, Framebuffer fb);

uint32
add_entity(Scene *scene, uint32 mask);

void
update_transforms(Scene *scene, real64 d_t);

Scene *
initialize_scene(AppMemory *mem);

uint32
get_entity_from_screen_pos(int32 x, int32 y, Scene *scene);

void
highlight_entity(Scene *scene, Framebuffer fb, uint32 id);

inline void
scene_reset_predictions(Scene *scene) {
    memcpy(scene->predicted_transforms, scene->transforms, scene->entity_count * sizeof(*scene->transforms));
    memcpy(scene->predicted_movements, scene->movements, scene->entity_count * sizeof(*scene->movements));
    memcpy(scene->predicted_collisions, scene->collisions, scene->entity_count * sizeof(*scene->collisions));
}

#define SCENE_WHEEL_H
#endif
