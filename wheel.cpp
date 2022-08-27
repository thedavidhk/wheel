#include <stdio.h>  // for printf (debug only)

// TODO:
// 
// - What is the app about?
// - What do I need?

#include "wheel.h"
#include "files_wheel.h"
#include "scene_wheel.h"

struct AppState {
    Scene *current_scene;
    Texture testimg;
    Font test_font;
    real64 app_time;
    int32 mouse_x;
    int32 mouse_y;
    uint32 frame_count;
    uint32 fps;
};

static Font
load_bitmap_font(const char* filename, AppMemory *mem, uint32 cwidth, uint32 cheight, uint32 ascii_offset) {
    Font font = {};

    font.bitmap = load_bmp_file(filename, mem);
    font.cwidth = cwidth;
    font.cheight = cheight;
    font.ascii_offset = ascii_offset;

    return font;
}

static Texture
create_string_texture(const char* str, AppMemory *mem, Font f, v4 color) {
    uint32 width = 0;
    uint32 max_width = 0;
    uint32 height = 1;
    const char *str_orig = str;
    while (*str) {
        if (*str == '\n') {
            width = 0;
            height++;
        }
        else {
            width++;
            max_width = max(width, max_width);
        }
        str++;
    }
    width *= f.cwidth;
    height *= f.cheight;
    Texture texture = {};
    texture.pixels = (uint32 *)get_memory(mem, sizeof(uint32) * width * height);
    texture.width = width;
    texture.height = height;
    draw_string_to_texture(&texture, str_orig, f, color);
    return texture;
}


AppHandle
initialize_app() {
    // TODO: This is completely arbitrary!
    static constexpr uint32 mem_size = megabytes(64);
    AppMemory *mem = initialize_memory(mem_size);

    AppState *as = (AppState *)get_memory(mem, sizeof(AppState));
    Scene *scene = initialize_scene(mem);
    as->current_scene = scene;

#if NEW_PHYSICS_SYSTEM == 0
    // DEBUG
    as->testimg = load_bmp_file("test.bmp", mem);
    as->test_font = load_bitmap_font("source_sans_pro.bmp", mem, 38, 64, 32);
    //const char *string = "Hi,\nThis is David.";


    Vertexbuffer *vb = scene->vertexbuffer;
    Indexbuffer *ib = scene->indexbuffer;

    // Initialize player.
    constexpr uint32 count_v = 4;
    Vertex data[count_v] = {
        {{-0.2, -0.2}, {0.3, 0.3, 0.3, 0.3}, {0, 0}},
        {{ 0.2, -0.2}, {0.3, 0.3, 0.3, 0.3}, {6, 0}},
        {{ 0.2,  0.2}, {0.3, 0.3, 0.3, 0.3}, {6, 1}},
        {{-0.2,  0.2}, {0.3, 0.3, 0.3, 0.3}, {0, 1}}
    };
    Vertex floor_data[count_v] = {
        {{-2.5, -0.2}, {0.3, 0.3, 0.3, 0.3}, {0, 0}},
        {{ 2.5, -0.2}, {0.3, 0.3, 0.3, 0.3}, {6, 0}},
        {{ 2.5,  0.2}, {0.3, 0.3, 0.3, 0.3}, {6, 1}},
        {{-2.5,  0.2}, {0.3, 0.3, 0.3, 0.3}, {0, 1}}
    };
    //Vertex data[count_v] = {
    //    {-0.5, -0.5},
    //    { 0.5, -0.3},
    //    {-0.5,  0.5}
    //};
    constexpr uint32 count_i = 6;
    uint32 indices[count_i] = {
        0, 1, 2, 0, 2, 3
    };

    int32 scenery_mask = CM_Pos | CM_Mesh | CM_Color | CM_Collision | CM_Rotation;

    //uint32 floor = add_entity(scene, scenery_mask | CM_Texture);
    uint32 floor = add_entity(scene, scenery_mask);
    scene->transforms[floor].pos = {0, 2};
    scene->transforms[floor].rot = 0.0;
    scene->transforms[floor].scale = {1, 1};
    scene->colors[floor] = {0.8, 0.4, 0.4, 0.5};
    scene->movements[floor] = {};
    //scene->textures[floor] = create_string_texture(string, mem, as->test_font, {1, 0.5, 1, 1});
    scene->meshes[floor] = create_polygon(vb, ib, floor_data, count_v, indices, count_i);
    scene->materials[floor] = {2, 3, 0.5};
    scene->floor_index = floor;


    /*
    uint32 wall = add_entity(scene, scenery_mask);
    scene->transforms[wall].pos = {1, 1};
    scene->colors[wall] = {0.3, 0.7, 1, 0.8};
    scene->meshes[wall] = create_polygon(vb, ib, data, count_v, indices, count_i);
    */

    int32 moveable_mask = CM_Pos | CM_Mesh | CM_Color | CM_Collision | CM_Velocity | CM_Mass | CM_Friction;
    uint32 pebble = add_entity(scene, moveable_mask);
    scene->transforms[pebble].pos = {-2, 1};
    scene->transforms[pebble].rot = 45 * PI / 180;
    scene->transforms[pebble].scale = {1, 1};
    scene->colors[pebble] = {1, 1, 1, 0.5};
    scene->meshes[pebble] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->masses[pebble].value = 1;
    scene->materials[scene->player_index] = {2, 3, 0.5};

    int32 player_mask = CM_Pos | CM_Mesh | CM_Color | CM_Velocity | CM_Force | CM_Mass | CM_Friction | CM_Collision | CM_Rotation | CM_Selectable;
    scene->player_index = add_entity(scene, player_mask);
    scene->meshes[scene->player_index] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->transforms[scene->player_index].scale = {1, 1};
    scene->forces[scene->player_index] = 5;
    scene->masses[scene->player_index].value = 1;
    scene->colors[scene->player_index] = {1, 1, 1, 0.5};
    scene->materials[scene->player_index] = {2, 3, 0.5};

#endif

    return (AppHandle)mem;
}

void
app_update_and_render(real64 frame_time, AppHandle app, Framebuffer fb) {
    AppMemory *mem = (AppMemory *)app;
    AppState *as = (AppState *)mem->data;
    Scene *scene = as->current_scene;

    // PHYSICS
    real64 time_left = frame_time;
    while (time_left > 0.0) {
        real64 d_t = min(frame_time, 1.0d / SIM_RATE);
        //uint32 f = scene->floor_index;

        if (!scene->paused) {
            //scene->transforms[f].rot = 10.0f * (PI / 180) * sin((real32)scene->scene_time);
            //accelerate(scene->player_movement, 1.0f * scene->forces[p], scene->masses[p].value, &scene->movements[p], d_t);
            apply_external_forces(scene->movements, scene->materials, scene->collisions, scene->entities, scene->entity_count, d_t);
            update_velocities(scene->movements, scene->entities, scene->entity_count, d_t);
            handle_collisions(scene, d_t);
            update_transforms(scene, d_t);
            scene->scene_time += d_t;
        }
        time_left -= d_t;
    }

    // RENDER
    clear_framebuffer(fb, {0});
    draw_scene(scene, fb);

    as->app_time += frame_time;
    as->frame_count++;

    if (as->app_time > 1.0f) {
        as->fps = round((as->frame_count / as->app_time));
        as->frame_count = 0;
        as->app_time = 0;
        char str[64];
        sprintf(str, "FPS: %2d", as->fps);
        printf("%s\n", str);
    }

    /*
    if (frame_time > 1.0f / ((double)FRAME_RATE) + 0.006f) {
        printf("Frame took too long: %6.4fs\n", frame_time);
    }
    */
}

void
key_callback(KeyBoardInput key, InputType t, AppHandle app) {
    AppState *as = (AppState *)(((AppMemory *)app)->data);
    Scene *scene = as->current_scene;
    v2 dir = {};
    int32 mag = (t == IT_PRESSED);
    switch (key) {
        case KEY_W:
            dir += mag * v2{ 0,-1};
            break;
        case KEY_A:
            dir += mag * v2{-1, 0};
            break;
        case KEY_S:
            dir += mag * v2{ 0, 1};
            break;
        case KEY_D:
            dir += mag * v2{ 1, 0};
            break;
        case KEY_SPACE:
            if (t == IT_PRESSED) {
                scene->paused = !scene->paused;
                if (scene->paused)
                    printf("Scene paused.\n");
                else
                    printf("Scene resumed.\n");
            }
            break;
        default:
            break;
    }
    if (magnitude(dir) > EPSILON)
        apply_impulse(scene->meshes[scene->player_index], scene->masses[scene->player_index].value, scene->transforms[scene->player_index].pos, &scene->movements[scene->player_index].v, center_of_mass(scene->meshes[scene->player_index]), dir * scene->forces[scene->player_index]);
    //scene->player_movement += dir;
}

void
mouse_button_callback(MouseButton btn, InputType t, int32 x, int32 y, AppHandle app) {
    AppState *as = (AppState *)((AppMemory *)app)->data;
    Scene *scene = as->current_scene;
    switch (btn) {
    case BUTTON_1:
        if (t == IT_PRESSED && (scene->entities[scene->hovered_entity] & CM_Selectable)) {
            scene->selected_entity = scene->hovered_entity;
        }
        else {
            scene->selected_entity = 0;
        }
        break;
    case BUTTON_4:
        scene->camera.scale *= 1.1;
        break;
    case BUTTON_5:
        scene->camera.scale *= 0.9;
        break;
    default:
        break;
    }
}

void
mouse_move_callback(int32 x, int32 y, uint32 mask, AppHandle app) {
    AppState *as = (AppState *)((AppMemory *)app)->data;
    Scene *scene = as->current_scene;
    uint32 entity_under_cursor = get_entity_from_screen_pos(x, y, scene);
    if (entity_under_cursor != scene->hovered_entity) {
        scene->hovered_entity = entity_under_cursor;
    }
    v2 diff_world = screen_to_world_space({(real32)x, (real32)y}, scene->camera) - screen_to_world_space({(real32)as->mouse_x, (real32)as->mouse_y}, scene->camera);
    if (scene->selected_entity) {
    // TODO: define masks ourselves
        if (mask & 1) {
            scene->transforms[scene->selected_entity].pos += diff_world;
        }
        else if (scene->entities[scene->selected_entity] & CM_Velocity) {
            scene->movements[scene->selected_entity].v += diff_world;
            scene->predicted_transforms[scene->selected_entity] = scene->transforms[scene->selected_entity];
            scene->predicted_transforms[scene->selected_entity].pos += scene->movements[scene->selected_entity].v;
        }
    }
    else if (mask & (1<<8)) {
        scene->camera.pos -= diff_world;
    }
    as->mouse_x = x;
    as->mouse_y = y;
}

