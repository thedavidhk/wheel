#include <stdio.h>
#include <cstdlib>

#include "handmade.h"

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
    game_state->player_x = 100;
    game_state->player_y = 200;
    game_state->player_speed = 200;
    game_state->player_size = 40;

    return mem;
}

void
gameUpdateAndRender(double d_t, GameMemory mem, GameInput *input, PixelBuffer buffer) {
    GameState *game_state = (GameState *)mem.data;
    int speed = game_state->player_speed;
    int player_size = game_state->player_size;
    if (input->up)
        game_state->player_y -= speed * d_t;
    else if (input->down)
        game_state->player_y += speed * d_t;
    else if (input->left)
        game_state->player_x -= speed * d_t;
    else if (input->right)
        game_state->player_x += speed * d_t;

    clearScreen(buffer, 0);
    drawRectangle(buffer,
                  game_state->player_x,
                  game_state->player_y,
                  game_state->player_x+player_size,
                  game_state->player_y+player_size,
                  0xFFFF0000);

    if (d_t > 1.0f/(FRAME_RATE-1))
        printf("Frame rate drop: %.2f FPS\n", 1/d_t);
}

