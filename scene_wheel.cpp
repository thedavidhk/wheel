#include "scene_wheel.h"
#include <stdio.h>

static void
draw_entities_wireframe(Scene *scene, Framebuffer fb);

void
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

Scene *
initialize_scene(AppMemory *mem) {
    Scene *scene = (Scene *)get_memory(mem, sizeof(Scene));

    // Initialize entity components.
    scene->max_entity_count = 64;
    //scene->entities = (uint32 *)get_memory(mem, scene->max_entity_count * sizeof(uint32));

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

    //add_entity(scene, 0); // NULL-Entity

    scene->paused = true;

    return scene;
}

