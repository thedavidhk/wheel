#include <stdio.h>
#include <unistd.h>
#include <cstdlib>

#include "handmade.h"
#include "math_handmade.h"

void
drawRectangle(PixelBuffer buffer, int startX, int startY, int endX, int endY, unsigned int color) {
    int pitch = buffer.width * buffer.bytes_per_pixel;
    for (int y = startY; y < endY; y++) {
        unsigned int *row = (unsigned int *)(buffer.data + y * pitch);
        for (int x = startX; x < endX; x++) {
            unsigned int *p = row + x;
            *p = color;
        }
    }
}

void
clearScreen(PixelBuffer buffer, unsigned int color) {
    drawRectangle(buffer, 0, 0, buffer.width, buffer.height, color);
}

GameMemory
initializeGame(unsigned long long mem_size) {
    GameMemory mem = {};
    mem.size = mem_size;
    mem.data = malloc(mem_size);
    if (!mem.data) {
        printf("Could not allocate game memory. Quitting...\n");
        exit(1);
    }

    GameState *game_state = (GameState *)mem.data;
    game_state->player.pos = {0.0f, 0.0f};
    game_state->player.color = 0xFFFF0000;
    game_state->player.speed = 100.0f;
    game_state->player.dim = {1.0f, 1.0f};
    game_state->camera.pos = {0.0f, 0.0f};
    game_state->camera.scale = 20.0f;

    return mem;
}

void
accelerate_2d(PhysicsObject2D *object, v2 acc) {
    // TODO
}

void
gameUpdateAndRender(double d_t, GameMemory mem, GameInput *input, PixelBuffer buffer) {
    GameState *game_state = (GameState *)mem.data;
    v2 player_size = game_state->player.dim;

    // Motion
    v2 acc = {};
    if (input->up)
        acc = acc + v2{0, -1};
    if (input->down)
        acc = acc + v2{0, 1};
    if (input->left)
        acc = acc + v2{-1, 0};
    if (input->right)
        acc = acc + v2{1, 0};
    float mag = magnitude(acc);
    acc = acc * game_state->player.speed;
    if (mag > 0.0001f)
        acc = acc / mag;

    const float drag_coef = 7.0f;
    acc = acc - game_state->player.vel * drag_coef;
    game_state->player.vel = game_state->player.vel + acc * d_t;
    game_state->player.pos = game_state->player.pos + game_state->player.vel * d_t;

    clearScreen(buffer, 0);

    // Convert world coordinates to screen coordinates
    v2 screen_quadrant = {WIN_WIDTH / 2, WIN_HEIGHT / 2};
    v2 player_screen_pos = (game_state->player.pos - game_state->camera.pos) * game_state->camera.scale + screen_quadrant;

    v2 player_size_pixels = player_size * game_state->camera.scale;

    rect player_pixels = rect(player_screen_pos - player_size_pixels / 2, player_screen_pos + player_size_pixels / 2);

    rect screen = rect({0, 0}, {WIN_WIDTH, WIN_HEIGHT});
    rect player_pixels_on_screen = intersection(player_pixels, screen);
    if (isPositive(player_pixels_on_screen)) {
        drawRectangle(buffer,
                (int) player_pixels_on_screen.min.x1,
                (int) player_pixels_on_screen.min.x2,
                (int) player_pixels_on_screen.max.x1,
                (int) player_pixels_on_screen.max.x2,
                game_state->player.color);
    }

    if (d_t > 1.0f/(FRAME_RATE-1))
        printf("Frame rate drop: %.2f FPS\n", 1/d_t);
}

