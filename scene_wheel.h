#ifndef SCENE_WHEEL_H

#include "shape_wheel.h"
#include "physics_wheel.h"
#include "render_wheel.h"
#include "memory_wheel.h"

#define MAX_VERTEX_COUNT 128
#define MAX_BODIES_PER_ENTITY 16
#define MAX_ENTITY_COUNT 64

struct DebugGrid {
    v4 bg_color;
    v4 primary_color;
    v4 secondary_color;
    v4 accent_color;
    v2 origin;
    v2 scale;
};

struct Entity {
    Body *bodies[MAX_BODIES_PER_ENTITY];
    uint32 body_count;
};

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
    uint32 vertex_count;
    v2 vertices[MAX_VERTEX_COUNT];
    v2 *vertices_unnecessary_copy;
    v2 normals[MAX_VERTEX_COUNT];
    uint32 collision_count;
    // Entities
    uint32 entity_count;
    uint32 max_entity_count;
    Entity entities[MAX_ENTITY_COUNT];
    ShapeList shapes;
    BodyList bodies;
};

inline uint32
scene_create_entitiy(Scene *scene) {
    assert(scene->entity_count < MAX_ENTITY_COUNT);
    Entity e = {};
    scene->entities[scene->entity_count] = e;
    return scene->entity_count++;
}

inline uint32
scene_create_polygon(Scene *scene, uint32 count, v2 *vertices) {
    assert(scene->vertex_count + count < MAX_VERTEX_COUNT);
    v2 *v_ptr = scene->vertices + scene->vertex_count;
    v2 *n_ptr = scene->normals + scene->vertex_count;
    memcpy(v_ptr, vertices, count * sizeof(*vertices));
    scene->vertex_count += count;
    return shape_create_polygon(&scene->shapes, count, v_ptr, n_ptr);
}

inline uint32
scene_create_body(Scene *scene, BodyDef def) {
    return physics_create_body(&scene->bodies, def);
}

inline void
scene_link_shape_to_body(Scene *scene, uint32 shape, uint32 body) {
    physics_link_shape_to_body(&scene->bodies.bodies[body], &scene->shapes.shapes[shape]);
}

inline void
scene_link_body_to_entity(Scene *scene, uint32 body, uint32 entity) {
    uint32 *body_count = &scene->entities[entity].body_count;
    assert(*body_count < MAX_BODIES_PER_ENTITY);
    scene->entities[entity].bodies[(*body_count)++] = &scene->bodies.bodies[body];
}

inline void
scene_draw_bodies(Scene *scene, Framebuffer fb, AppMemory *mem) {
    scene->vertices_unnecessary_copy = (v2 *)get_memory(mem, 100 * scene->vertex_count * sizeof(v2));
    char *unnecessary_string = (char *)get_memory(mem, 2000);
    for (uint32 i = 0; i < scene->bodies.count; i++) {
        Body *b = &scene->bodies.bodies[i];
        for (uint32 j = 0; j < b->shape_count; j++) {
            renderer_draw_shape_to_buffer(fb, scene->camera, *b->shapes[j], b->p, b->p_ang);
        }
    }
    free_memory(mem, scene->vertices_unnecessary_copy, 100 * scene->vertex_count * sizeof(v2));
    char *unnecessary_string2 = (char *)get_memory(mem, 2000);
    free_memory(mem, unnecessary_string2, 2000);
    free_memory(mem, unnecessary_string, 2000);
}

void
draw_grid(Scene *scene, Framebuffer fb);

void
draw_scene(Scene *scene, Framebuffer fb);

Scene *
initialize_scene(AppMemory *mem);

uint32
get_entity_from_screen_pos(int32 x, int32 y, Scene *scene);

void
highlight_entity(Scene *scene, Framebuffer fb, uint32 id);

#define SCENE_WHEEL_H
#endif
