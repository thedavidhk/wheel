#include <stdio.h>  // for printf (debug only)

#include "wheel.h"
#include "scene_wheel.h"

struct AppState {
    Scene *current_scene;
    Texture testimg;
    Texture string_texture;
    Font test_font;
    real64 app_time;
    int32 mouse_x;
    int32 mouse_y;
    uint32 frame_count;
    uint32 fps;
};

static Texture
load_bmp_file(const char* filename, AppMemory *mem) {
    Texture f;
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
    for (uint32 y = f.height; y > 0; y--) {
        fread(f.pixels + y * f.width - 1, 4, f.width, ptr);
    }
    fclose(ptr);
    return f;
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

    // DEBUG
    as->testimg = load_bmp_file("test.bmp", mem);
    as->test_font = load_bitmap_font("source_sans_pro.bmp", mem, 38, 64, 32);
    const char *string = "Hi,\nThis is David.";
    as->string_texture = create_string_texture(string, mem, as->test_font, {1, 1, 1, 1});
    //


    Vertexbuffer *vb = scene->vertexbuffer;
    Indexbuffer *ib = scene->indexbuffer;

    // Initialize player.
    constexpr uint32 count_v = 3;
    Vertex data[count_v] = {
        {{ 0.0, -0.5}, {1, 0, 0, 1}},
        {{ 0.5,  0.5}, {0, 1, 0, 1}},
        {{-0.5,  0.5}, {0, 0, 1, 1}}
    };
    //Vertex data[count_v] = {
    //    {-0.5, -0.5},
    //    { 0.5, -0.3},
    //    {-0.5,  0.5}
    //};
    constexpr uint32 count_i = 3;
    uint32 indices[count_i] = {
        0, 1, 2
    };


    int32 scenery_mask = CM_Pos | CM_Mesh | CM_Color | CM_Collision | CM_Rotation;
    uint32 wall = add_entity(scene, scenery_mask);
    scene->transforms[wall].pos = {1, 1};
    scene->colors[wall] = {0.3, 0.7, 1, 0.8};
    scene->meshes[wall] = create_polygon(vb, ib, data, count_v, indices, count_i);

    uint32 floor = add_entity(scene, scenery_mask);
    scene->transforms[floor].pos = {0, 6};
    scene->transforms[floor].rot = 0.2;
    scene->colors[floor] = {1, 1, 1, 0.8};
    scene->meshes[floor] = create_rectangle(vb, ib, {-20, -0.5}, {20, 0.5}, scene->colors[floor]);

    int32 moveable_mask = CM_Pos | CM_Mesh | CM_Color | CM_Collision | CM_Velocity | CM_Mass | CM_Friction;
    uint32 pebble = add_entity(scene, moveable_mask);
    scene->transforms[pebble].pos = {6, -1};
    scene->colors[pebble] = {0.3, 0.7, 0.2, 0.8};
    scene->meshes[pebble] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->movements[pebble].v = {-10, 0};
    scene->masses[pebble].value = 1;
    scene->frictions[pebble] = 1;

    int32 player_mask = CM_Pos | CM_Mesh | CM_Color | CM_Velocity | CM_Force | CM_Mass | CM_Friction | CM_Collision | CM_Rotation;
    scene->player_index = add_entity(scene, player_mask);
    scene->transforms[scene->player_index].rot = 0.3;
    scene->colors[scene->player_index] = {1, 0.7, 0.3, 0.8};
    scene->meshes[scene->player_index] = create_polygon(vb, ib, data, count_v, indices, count_i);
    scene->forces[scene->player_index] = 2;
    scene->masses[scene->player_index].value = 1;
    scene->frictions[scene->player_index] = 1;

    scene_reset_predictions(scene);

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
#if 0
        update_velocities(scene, d_t);
        handle_collisions(scene, d_t);
        if (!scene->paused) {
            update_transforms(scene, d_t);
        }
#else
        if (!scene->paused) {
            //scene_reset_predictions(scene);
            accelerate(scene->player_movement, 1.0f * scene->forces[p], scene->masses[p].value, &scene->movements[p], d_t);
            update_velocities(scene, d_t);
            handle_collisions(scene, d_t);
            update_transforms(scene, d_t);
        }
#endif
        time_left -= d_t;
    }

    // RENDER
    clear_framebuffer(fb, {0});
    draw_entities(scene, fb);



    //DEBUG bitmaps

    draw_texture_alpha(as->testimg, fb, 20, 20);
    draw_texture_alpha(as->string_texture, fb, 200, 200);


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
        as->fps = round((as->frame_count / as->app_time));
        as->frame_count = 0;
        as->app_time = 0;
    }

    char str[64];
    sprintf(str, "FPS: %2d", as->fps);
    render_text(fb, str, as->test_font, {v2{400, 0}, 0.5, 0}, {1, 1, 1, 1});
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

