#include "scene_wheel.h"
#include <stdio.h>

static void
draw_entities_wireframe(Scene *scene, Framebuffer fb);

static void
draw_grid(Scene *scene, Framebuffer fb);

#if NEW_PHYSICS_SYSTEM == 0
void
handle_collisions(Scene *scene, real64 d_t) {
    uint32 collision_count = 0;
    Collision collisions[64] = {0};
    uint32 iteration_count = 0;
    static constexpr int32 MASK = CM_Pos | CM_Collision;
    bool first_check = true;
    bool collisions_found = false;

    while (first_check || collisions_found) {
        iteration_count++;
        for (uint32 i = 1; i < scene->entity_count; i++) {
            if ((scene->entities[i] & (MASK)) != (MASK))
                continue;
            for (uint32 j = i + 1; j < scene->entity_count; j++) {
                // TODO: Clean this up!! Which entities do I need to check with which others to not get duplicate collisions?
                if (((scene->entities[j] & MASK) != MASK))
                    continue;
                Collision collision = {0};
                collision.v_a = &scene->movements[i].v;
                collision.v_b = &scene->movements[j].v;
                collision.m_a = scene->masses[i].value;
                collision.m_b = scene->masses[j].value;
                collision.mat_a = scene->materials[i];
                collision.mat_b = scene->materials[j];
                if (first_check) {
                    check_collision(&collision, scene->meshes[i], scene->meshes[j], scene->transforms[i], scene->transforms[j], scene->movements[i].v, scene->movements[j].v, d_t, true);
                }
                else {
                    Transform t_a_future = scene->transforms[i];
                    Transform t_b_future = scene->transforms[j];
                    t_a_future.pos += scene->movements[i].v * d_t;
                    t_b_future.pos += scene->movements[j].v * d_t;
                    check_collision(&collision, scene->meshes[i], scene->meshes[j], t_a_future, t_b_future, scene->movements[i].v, scene->movements[j].v, d_t, false);
                }
                if (!(scene->entities[j] & CM_Velocity)) {
                    collision.v_b = 0;
                }
                if (!(scene->entities[i] & CM_Velocity)) {
                    collision.v_a = 0;
                }
                if (collision.collided) {
                    collisions[collision_count++] = collision;
                }
            }
        }
        collisions_found = collision_count > 0;
        while (collision_count) {
            resolve_collision(&collisions[--collision_count]);
        }
        first_check = false;
    }
    // Idea:
    // Turn this into a loop:
    // 1. First do a sweeping collision check to detect all collisions in t and t+1.
    // 2. Resolve all collisions by adjusting velocities
    // 3. Do a collision check for t+1 only to check if resolution has worked
    // 4. Are there collisions still? Go back to 2. Else finish.
}
#endif

void
draw_scene(Scene *scene, Framebuffer fb) {
    draw_grid(scene, fb);
    draw_entities(scene, fb);
}

static void
draw_grid(Scene *scene, Framebuffer fb) {
    // Draw grid
    clear_framebuffer(fb, scene->grid.bg_color);
    uint32 n_grid_lines_x = (uint32) (scene->camera.width / (scene->camera.scale * scene->grid.scale.x)) + 1;
    uint32 n_grid_lines_y = (uint32) (scene->camera.height / (scene->camera.scale * scene->grid.scale.y)) + 1;
    real32 dx = scene->camera.scale * scene->grid.scale.x;
    real32 dy = scene->camera.scale * scene->grid.scale.y;
    real32 x = (real32) (((int32)(0.5 * scene->camera.width - scene->camera.pos.x * scene->camera.scale)) % ((int32) dx)) - dx;
    real32 y = (real32) (((int32)(0.5 * scene->camera.height - scene->camera.pos.y * scene->camera.scale)) % ((int32) dy)) - dy;
    for (uint32 i = 0; i <= n_grid_lines_x; i++) {
        draw_line(fb, {x, 0}, {x, (real32) scene->camera.height}, scene->grid.primary_color);
        if (scene->camera.scale * scene->grid.scale.x > 100) {
            for (uint32 j = 1; j < 10; j++) {
                real32 ddx = j * dx / 10;
                draw_line(fb, {x + ddx, 0}, {x + ddx, (real32) scene->camera.height}, scene->grid.secondary_color);
            }
        }
        x += dx;
    }
    for (uint32 i = 0; i <= n_grid_lines_y; i++) {
        draw_line(fb, {0, y}, {(real32) scene->camera.width, y}, scene->grid.primary_color);
        if (scene->camera.scale * scene->grid.scale.y > 100) {
            for (uint32 j = 1; j < 10; j++) {
                real32 ddy = j * dy / 10;
                draw_line(fb, {0, y + ddy}, {(real32) scene->camera.width, y + ddy}, scene->grid.secondary_color);
            }
        }
        y += dy;
    }
    v2 origin = world_to_screen_space({0, 0}, scene->camera);
    draw_line(fb, {origin.x, 0}, {origin.x, (real32) scene->camera.height}, scene->grid.accent_color, 5);
    draw_line(fb, {0, origin.y}, {(real32) scene->camera.width, origin.y}, scene->grid.accent_color, 5);
}

void
draw_entities(Scene *scene, Framebuffer fb) {
    static constexpr int32 MASK = CM_Mesh | CM_Pos | CM_Color;
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            if (scene->entities[i] & CM_Texture)
                draw_mesh(scene->camera, fb, scene->meshes[i], scene->transforms[i], &scene->textures[i], 0);
            else
                draw_mesh(scene->camera, fb, scene->meshes[i], scene->transforms[i], 0, &scene->colors[i]);

            // DEBUG
            //v2 vel_screen = world_to_screen_space(scene->movements[i].v, scene->camera);
            //v2 pos_screen = world_to_screen_space(transform(center_of_mass(scene->meshes[i]), scene->transforms[i]), scene->camera);
            //v2 origin_screen = world_to_screen_space({}, scene->camera);
            //debug_draw_vector(fb, vel_screen - origin_screen, pos_screen, color_opaque(scene->colors[i]));
            //debug_draw_vector(fb, world_to_screen_space(scene->predicted_collisions[i].mtv, scene->camera) - origin_screen, world_to_screen_space(scene->predicted_collisions[i].poi, scene->camera), {1, 0, 0, 1});
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
update_transforms(Scene *scene, real64 d_t) {
    static constexpr int32 MASK = (CM_Pos | CM_Velocity);
    for (uint32 i = 1; i < scene->entity_count; i++) {
        if ((scene->entities[i] & MASK) == MASK) {
            scene->transforms[i].pos += scene->movements[i].v * d_t;
            scene->transforms[i].rot += scene->movements[i].v_ang * d_t;
        }
    }
}

Scene *
initialize_scene(AppMemory *mem) {
    Scene *scene = (Scene *)get_memory(mem, sizeof(Scene));
    scene->vertexbuffer = (Vertexbuffer *)get_memory(mem, sizeof(Vertexbuffer));
    scene->indexbuffer = (Indexbuffer *)get_memory(mem, sizeof(Indexbuffer));

    // Initialize entity components.
    scene->max_entity_count = 64;
    scene->entities = (uint32 *)get_memory(mem, scene->max_entity_count * sizeof(uint32));

#if NEW_PHYSICS_SYSTEM == 0
    // Initialize vertex and index buffer.
    static constexpr uint32 MAX_V = 64;
    scene->vertexbuffer->data = (Vertex *)get_memory(mem, MAX_V * sizeof(Vertex));
    scene->vertexbuffer->max_count = MAX_V;
    scene->vertexbuffer->count = 0;
    static constexpr uint32 MAX_I = 64;
    scene->indexbuffer->indices = (uint32 *)get_memory(mem, MAX_I * sizeof(uint32));
    scene->indexbuffer->max_count = MAX_I;
    scene->indexbuffer->count = 0;

    scene->transforms = (Transform *)get_memory(mem, scene->max_entity_count * sizeof(Transform));
    scene->predicted_transforms = (Transform *)get_memory(mem, scene->max_entity_count * sizeof(Transform));
    scene->meshes = (Mesh *)get_memory(mem, scene->max_entity_count * sizeof(Mesh));
    scene->colors = (v4 *)get_memory(mem, scene->max_entity_count * sizeof(v4));
    scene->textures = (Texture *)get_memory(mem, scene->max_entity_count * sizeof(Texture));
    scene->movements = (Movement *)get_memory(mem, scene->max_entity_count * sizeof(Movement));
    scene->predicted_movements = (Movement *)get_memory(mem, scene->max_entity_count * sizeof(Movement));
    //scene->frictions = (Friction *)get_memory(mem, scene->max_entity_count * sizeof(Friction));
    scene->materials = (Material *)get_memory(mem, scene->max_entity_count * sizeof(Material));
    scene->forces = (real32 *)get_memory(mem, scene->max_entity_count * sizeof(real32));
    scene->masses = (Mass *)get_memory(mem, scene->max_entity_count * sizeof(Mass));
    scene->collisions = (Collision *)get_memory(mem, scene->max_entity_count * sizeof(Collision));
    scene->predicted_collisions = (Collision *)get_memory(mem, scene->max_entity_count * sizeof(Collision));
#endif

    // Initialize camera.
    scene->camera.pos = {0.0f, 0.0f};
    scene->camera.scale = 100.0f;
    scene->camera.width = WIN_WIDTH;
    scene->camera.height = WIN_HEIGHT;

    scene->grid = {
        {0.1, 0.1, 0.1, 1}, // bg_color
        {0.5, 0.5, 0.5, 1}, // primary_color
        {0.2, 0.2, 0.3, 1}, // secondary_color
        {0.6, 0.4, 0.0, 1}, // accent_color
        {0, 0},             // origin
        {1, 1}              // scale
    };

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

