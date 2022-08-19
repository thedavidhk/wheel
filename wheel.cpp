#include <stdio.h>  // for printf (debug only)

#include "wheel.h"
#include "scene_wheel.h"

struct AppState {
    Scene *current_scene;
    Bitmap testimg;
    int32 mouse_x;
    int32 mouse_y;
    uint32 frame_count;
    real64 app_time;
};

static Bitmap
load_bmp_file(const char* filename, AppMemory *mem) {
    Bitmap f;
    FILE *ptr;
    ptr = fopen(filename, "rb");
    if (!ptr) {
        printf("File %s could not be opened.\n", filename);
        exit(1);
    }
    uint8 header[26];
    if (!fread(header, 1, 26, ptr)) {
        printf("File %s could not be read.\n", filename);
        exit(1);
    }
    // TODO: More careful compatibility checking
    uint32 fsize = *(uint32 *)(header + 2);
    uint32 offset = *(uint32 *)(header + 10);
    f.width = *(uint32 *)(header + 18);
    f.height = *(uint32 *)(header + 22);
    f.bytes = (uint8 *)get_memory(mem, (fsize - offset));
    fseek(ptr, offset, SEEK_SET);
    uint32 nbytes_read = fread(f.bytes, 1, fsize - offset, ptr);
    assert(nbytes_read == fsize - offset);
    printf("%d bytes read from file %s.\n", fsize - offset, filename);
    fclose(ptr);
    return f;
};


AppHandle
initialize_app() {
    // TODO: This is completely arbitrary!
    static constexpr uint32 mem_size = megabytes(64);
    AppMemory *mem = initialize_memory(mem_size);

    AppState *as = (AppState *)get_memory(mem, sizeof(AppState));
    Scene *scene = initialize_scene(mem);
    as->current_scene = scene;
    as->testimg = load_bmp_file("test.bmp", mem);

    Vertexbuffer *vb = scene->vertexbuffer;
    Indexbuffer *ib = scene->indexbuffer;

    // Initialize player.
    constexpr uint32 count_v = 3;
    v2 data[count_v] = {
        {-0.5, -0.5},
        { 0.5, -0.3},
        {-0.5,  0.5}
    };
    constexpr uint32 count_i = 3;
    uint32 indices[count_i] = {
        0, 1, 2
    };


    int32 scenery_mask = CM_Pos | CM_Mesh | CM_Color | CM_Collision | CM_Rotation;
    uint32 wall = add_entity(scene, scenery_mask);
    scene->transforms[wall].pos = {1, 1};
    scene->meshes[wall] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->colors[wall] = {0.3, 0.7, 1, 0.8};

    uint32 floor = add_entity(scene, scenery_mask);
    scene->transforms[floor].pos = {0, 6};
    scene->transforms[floor].rot = 0.2;
    scene->meshes[floor] = create_rectangle(vb, ib, {-20, -0.5}, {20, 0.5});
    scene->colors[floor] = {1, 1, 1, 0.8};

    int32 moveable_mask = CM_Pos | CM_Mesh | CM_Color | CM_Collision | CM_Velocity | CM_Mass | CM_Friction;
    uint32 pebble = add_entity(scene, moveable_mask);
    scene->transforms[pebble].pos = {6, -1};
    scene->meshes[pebble] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->colors[pebble] = {0.3, 0.7, 0.2, 0.8};
    scene->movements[pebble].v = {-10, 0};
    scene->masses[pebble].value = 1;
    scene->frictions[pebble] = 1;

    int32 player_mask = CM_Pos | CM_Mesh | CM_Color | CM_Velocity | CM_Force | CM_Mass | CM_Friction | CM_Collision | CM_Rotation;
    scene->player_index = add_entity(scene, player_mask);
    scene->meshes[scene->player_index] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->colors[scene->player_index] = {1, 0.7, 0.3, 0.8};
    scene->forces[scene->player_index] = 2;
    scene->masses[scene->player_index].value = 1;
    scene->frictions[scene->player_index] = 1;

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
        uint32 p = scene->player_index;
        if (!scene->paused) {
            accelerate(scene->player_movement, 1.0f * scene->forces[p], scene->masses[p].value, &scene->movements[p], d_t);
            update_velocities(scene, d_t);
            handle_collisions(scene, d_t);
            update_transforms(scene, d_t);
        }
        time_left -= d_t;
    }

    // RENDER
    clear_framebuffer(fb, {0});
    draw_entities(scene, fb);


    //DEBUG bitmaps

    draw_bitmap_alpha(as->testimg, fb, 20, 20);


    //draw_entities_wireframe(scene, fb);
    /*
    uint32 h = scene->hovered_entity;
    if (h)
        highlight_entity(scene, fb, h);
    uint32 s = scene->selected_entity;
    if (s)
        highlight_entity(scene, fb, s);
    */

    as->app_time += frame_time;
    as->frame_count++;
    if (as->app_time > 1.0f) {
        uint32 fps = round((as->frame_count / as->app_time));
        printf("FPS: %d\n", fps);
        as->frame_count = 0;
        as->app_time = 0;
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
    int32 mag = 2 * (t == IT_PRESSED) - 1;
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
                printf("Scene paused.\n");
                scene->paused = !scene->paused;
            }
            break;
        default:
            break;
    }
    scene->player_movement += dir;
}

void
mouse_button_callback(MouseButton btn, InputType t, int32 x, int32 y, AppHandle app) {
    AppState *as = (AppState *)((AppMemory *)app)->data;
    Scene *scene = as->current_scene;
    switch (btn) {
    case BUTTON_1:
        if (t == IT_PRESSED) {
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
        if (mask & 1)
            scene->transforms[scene->selected_entity].pos += diff_world;
        else if (scene->entities[scene->selected_entity] & CM_Velocity)
            scene->movements[scene->selected_entity].v += diff_world;
    }
    else if (mask & (1<<8)) {
        scene->camera.pos -= diff_world;
    }
    as->mouse_x = x;
    as->mouse_y = y;
}

