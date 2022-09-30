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
    static constexpr uint32 mem_size = megabytes(1);
    AppMemory *mem = initialize_memory(mem_size);

    AppState *as = (AppState *)get_memory(mem, sizeof(AppState));
    Scene *scene = initialize_scene(mem);
    as->current_scene = scene;

    uint32 player = scene_create_entitiy(scene);
    v2 poly_def[4] = {
        {-0.5, -0.5},
        {-0.5,  0.5},
        { 0.5,  0.5},
        { 0.5, -0.5}
    };
    uint32 poly = scene_create_polygon(scene, 4, poly_def);
    BodyDef body_def = {0};
    body_def.p = {-1, 0};
    body_def.m = 5;
    uint32 body = scene_create_body(scene, body_def);
    scene_link_shape_to_body(scene, poly, body);
    scene_link_body_to_entity(scene, body, player);

    scene->player_index = player;

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

        if (!scene->paused) {
            Body *player_body = scene->entities[scene->player_index].bodies[0];
            player_body->p_ang += d_t * 100 * PI / 180;
            scene->scene_time += d_t;
        }
        time_left -= d_t;
    }

    // RENDER
    clear_framebuffer(fb, {0});

    draw_grid(scene, fb);

    scene_draw_bodies(scene, fb, mem);

    as->app_time += frame_time;
    as->frame_count++;

    if (as->app_time > 1.0f) {
        as->fps = round((as->frame_count / as->app_time));
        as->frame_count = 0;
        as->app_time = 0;
        char str[64];
        sprintf(str, "FPS: %2d", as->fps);
        //printf("%s\n", str);

        uint64 mem_unit = 1;
        uint64 mem_multiple = mem->used_size;
        const char *desc[4] = {
            "B",
            "KB",
            "MB",
            "GB"
        };
        uint32 i = 0;
        while (mem_multiple / 1024) {
            mem_unit *= 1024;
            mem_multiple /= mem_unit;
            i++;
        }
        real32 mem_decimal = (real32) mem->used_size / mem_unit;
        real32 frag_decimal = (real32) mem->fragmented / mem_unit;
        printf("Total memory used: %4.2f %s\n", mem_decimal, desc[min(i, 3)]);
        printf("Fragmented memory: %4.2f %s\n", frag_decimal, desc[min(i, 3)]);
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
        ; // TODO: apply impulse to player body
}

void
mouse_button_callback(MouseButton btn, InputType t, int32 x, int32 y, AppHandle app) {
    AppState *as = (AppState *)((AppMemory *)app)->data;
    Scene *scene = as->current_scene;
    switch (btn) {
    case BUTTON_1:
        //if (t == IT_PRESSED && (scene->entities[scene->hovered_entity] & CM_Selectable)) {
        //    scene->selected_entity = scene->hovered_entity;
        //}
        //else {
        //    scene->selected_entity = 0;
        //}
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
    //uint32 entity_under_cursor = get_entity_from_screen_pos(x, y, scene);
    //if (entity_under_cursor != scene->hovered_entity) {
    //    scene->hovered_entity = entity_under_cursor;
    //}
    v2 diff_world = screen_to_world_space({(real32)x, (real32)y}, scene->camera) - screen_to_world_space({(real32)as->mouse_x, (real32)as->mouse_y}, scene->camera);
    if (scene->selected_entity) {
    // TODO: define masks ourselves
        //if (mask & 1) {
        //    //scene->transforms[scene->selected_entity].pos += diff_world;
        //    ;
        //}
        //else if (scene->entities[scene->selected_entity] & CM_Velocity) {
        //    //scene->movements[scene->selected_entity].v += diff_world;
        //    //scene->predicted_transforms[scene->selected_entity] = scene->transforms[scene->selected_entity];
        //    //scene->predicted_transforms[scene->selected_entity].pos += scene->movements[scene->selected_entity].v;
        //    ;
        //}
    }
    else if (mask & (1<<8)) {
        scene->camera.pos -= diff_world;
    }
    as->mouse_x = x;
    as->mouse_y = y;
}

