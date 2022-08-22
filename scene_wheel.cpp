#include "scene_wheel.h"
#include <stdio.h>

static void
draw_entities_wireframe(Scene *scene, Framebuffer fb);

void
handle_collisions(Scene *scene, real64 d_t) {
    static constexpr int32 MASK = CM_Pos | CM_Collision;
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & (MASK | CM_Velocity | CM_Mass)) != (MASK | CM_Velocity | CM_Mass))
            continue;
        for (uint32 j = 1; j < scene->entity_count; j++) {
            if (i == j || (scene->entities[j] & MASK) != MASK)
                continue;
#if 1
            scene->predicted_transforms[i] = scene->transforms[i];
            scene->predicted_transforms[i].pos += scene->movements[i].v;
            scene->collisions[i] = check_collision(scene->meshes[i], scene->meshes[j],
                    scene->transforms[i], scene->transforms[j], scene->movements[i],
                    scene->movements[j], d_t);
            scene->predicted_collisions[i] = check_collision(scene->meshes[i], scene->meshes[j],
                    scene->transforms[i], scene->transforms[j], scene->movements[i],
                    scene->movements[j], 1);
#else
            Collision col = check_collision(scene->meshes[i], scene->meshes[j],
                    scene->transforms[i], scene->transforms[j], scene->movements[i],
                    scene->movements[j], d_t);
#endif
            if (scene->predicted_collisions[i].collided) {
                resolve_collision(scene->predicted_collisions[i], scene->meshes[i], &scene->predicted_transforms[i], scene->transforms[j], &scene->predicted_movements[i], scene->masses[i]);
                scene->predicted_transforms[i].pos += scene->predicted_movements[i].v;
            }
            else {
                scene->predicted_collisions[i] = {};
            }
            if (scene->collisions[i].collided) {
#if 1
                resolve_collision(scene->collisions[i], scene->meshes[i], &scene->transforms[i], scene->transforms[j], &scene->movements[i], scene->masses[i]);
#else
                if (scene->entities[j] == (scene->entities[j] | CM_Velocity | CM_Mass)) {
                    resolve_collision(col, scene->meshes[i], scene->meshes[j], &scene->transforms[i], &scene->transforms[j], &scene->movements[i],
                            &scene->movements[j], scene->masses[i], scene->masses[j]);
                    scene->ghosts[i] = {};
                } else {
                    resolve_collision(col, scene->meshes[i], &scene->transforms[i], scene->transforms[j], &scene->movements[i],
                            scene->masses[i]);
                }
#endif
            }
            else {
                scene->collisions[i] = {};
            }
        }
    }
}

void
draw_entities(Scene *scene, Framebuffer fb) {
    static constexpr int32 MASK = CM_Mesh | CM_Pos | CM_Color;
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            if (scene->entities[i] & CM_Texture)
                draw_mesh(scene->camera, fb, scene->meshes[i], scene->transforms[i], &scene->textures[i]);
            else
                draw_mesh(scene->camera, fb, scene->meshes[i], scene->transforms[i], 0);

            // DEBUG
            v2 vel_screen = world_to_screen_space(scene->movements[i].v, scene->camera);
            v2 pos_screen = world_to_screen_space(transform(center_of_mass(scene->meshes[i]), scene->transforms[i]), scene->camera);
            v2 origin_screen = world_to_screen_space({}, scene->camera);
            debug_draw_vector(fb, vel_screen - origin_screen, pos_screen, color_opaque(scene->colors[i]));
            debug_draw_vector(fb, world_to_screen_space(scene->predicted_collisions[i].mtv, scene->camera) - origin_screen, world_to_screen_space(scene->predicted_collisions[i].poi, scene->camera), {1, 0, 0, 1});
            //draw_mesh_wireframe(scene->camera, fb, scene->meshes[i], scene->predicted_transforms[i], scene->colors[i], 1);
        }
    }
}

static void
draw_entities_wireframe(Scene *scene, Framebuffer fb) {
    static constexpr int32 MASK = CM_Mesh | CM_Pos | CM_Color;
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            draw_mesh_wireframe(scene->camera, fb, scene->meshes[i], scene->transforms[i], scene->colors[i], 1);

            /*
            // DEBUG
            v2 vel_screen = world_to_screen_space(scene->movements[i].v, scene->camera);
            v2 pos_screen = world_to_screen_space(transform(center_of_mass(scene->meshes[i]), scene->transforms[i]), scene->camera);
            v2 origin_screen = world_to_screen_space({}, scene->camera);
            debug_draw_vector(fb, vel_screen - origin_screen, pos_screen, scene->colors[i]);
            */
        }
    }
}

uint32
add_entity(Scene *scene, uint32 mask) {
    scene->entities[scene->entity_count] = mask;
    return scene->entity_count++;
}

void
update_velocities(Scene *scene, real64 d_t) {
    static constexpr int32 MASK = (CM_Pos | CM_Velocity);
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            scene->movements[i].v += scene->movements[i].a * d_t;
            scene->movements[i].a = {};
            //if (scene->entities[i] & CM_Mass) {
            //    scene->movements[i].v.y += 3 * d_t; // GRAVITY
            //}
            if (scene->entities[i] & CM_Friction) {
                scene->movements[i].v -= scene->movements[i].v * scene->frictions[i] * d_t;
                scene->movements[i].rot_v -= scene->movements[i].rot_v * scene->frictions[i] * d_t;
            }
            scene->predicted_movements[i].v += scene->predicted_movements[i].a * d_t;
            scene->predicted_movements[i].a = {};
            if (scene->entities[i] & CM_Friction) {
                scene->predicted_movements[i].v -= scene->predicted_movements[i].v * scene->frictions[i] * d_t;
                scene->predicted_movements[i].rot_v -= scene->predicted_movements[i].rot_v * scene->frictions[i] * d_t;
            }
        }
    }
}

void
update_transforms(Scene *scene, real64 d_t) {
    static constexpr int32 MASK = (CM_Pos | CM_Velocity);
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            scene->transforms[i].pos += scene->movements[i].v * d_t;
            scene->transforms[i].pos += scene->movements[i].v * d_t;
            scene->predicted_transforms[i].rot += scene->predicted_movements[i].rot_v * d_t;
            scene->predicted_transforms[i].rot += scene->predicted_movements[i].rot_v * d_t;
        }
    }
}

Scene *
initialize_scene(AppMemory *mem) {
    Scene *scene = (Scene *)get_memory(mem, sizeof(Scene));
    scene->vertexbuffer = (Vertexbuffer *)get_memory(mem, sizeof(Vertexbuffer));
    scene->indexbuffer = (Indexbuffer *)get_memory(mem, sizeof(Indexbuffer));

    // Initialize vertex and index buffer.
    static constexpr uint32 MAX_V = 64;
    scene->vertexbuffer->data = (Vertex *)get_memory(mem, MAX_V * sizeof(Vertex));
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
    scene->predicted_transforms = (Transform *)get_memory(mem, scene->max_entity_count * sizeof(Transform));
    scene->meshes = (Mesh *)get_memory(mem, scene->max_entity_count * sizeof(Mesh));
    scene->colors = (v4 *)get_memory(mem, scene->max_entity_count * sizeof(v4));
    scene->textures = (Texture *)get_memory(mem, scene->max_entity_count * sizeof(Texture));
    scene->movements = (Movement *)get_memory(mem, scene->max_entity_count * sizeof(Movement));
    scene->predicted_movements = (Movement *)get_memory(mem, scene->max_entity_count * sizeof(Movement));
    scene->forces = (real32 *)get_memory(mem, scene->max_entity_count * sizeof(real32));
    scene->masses = (Mass *)get_memory(mem, scene->max_entity_count * sizeof(Mass));
    scene->frictions = (real32 *)get_memory(mem, scene->max_entity_count * sizeof(real32));
    scene->collisions = (Collision *)get_memory(mem, scene->max_entity_count * sizeof(Collision));
    scene->predicted_collisions = (Collision *)get_memory(mem, scene->max_entity_count * sizeof(Collision));

    // Initialize camera.
    scene->camera.pos = {0.0f, 0.0f};
    scene->camera.scale = 100.0f;
    scene->camera.width = WIN_WIDTH;
    scene->camera.height = WIN_HEIGHT;

    add_entity(scene, 0); // NULL-Entity

    scene->paused = true;

    return scene;
}

uint32
get_entity_from_screen_pos(int32 x, int32 y, Scene *scene) {
    v2 pos = screen_to_world_space({(real32)x, (real32)y}, scene->camera);
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if (is_in_mesh(pos, scene->meshes[i], scene->transforms[i]))
            return i;
    }
    return 0;
}

void
highlight_entity(Scene *scene, Framebuffer fb, uint32 id) {
    draw_mesh_wireframe(scene->camera, fb, scene->meshes[id], scene->transforms[id], scene->colors[id], 3);
}

