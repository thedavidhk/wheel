#pragma once

#include "math_handmade.h"

#define kilobytes(value) ((value) * 1024)
#define megabytes(value) (kilobytes(value) * 1024)
#define gigabytes(value) (megabytes(value) * 1024)

#define FRAME_RATE 60
#define WIN_WIDTH 800
#define WIN_HEIGHT 600

struct GameMemory {
    unsigned long long size;
    void *data;
};

struct GameInput {
    bool up, down, left, right;
};

struct Player {
    v2 pos;
    v2 vel;
    v2 dim;
    float speed;
    unsigned int color;
};

struct PhysicsObject2D {
    v2 pos;
    v2 vel;
};

struct Camera {
    float scale; // pixels per meter
    v2 pos;
};

struct GameState {
    Player player;
    Camera camera;
};


struct PixelBuffer {
    char *data;
    int width, height, bytes_per_pixel;
};

void
drawRectangle(PixelBuffer buffer, v2 start, v2 end, unsigned int color);

void
clearScreen(PixelBuffer buffer, unsigned int color);

GameMemory
initializeGame(unsigned long long mem_size);

void
gameUpdateAndRender(double d_t, GameMemory mem, GameInput *input, PixelBuffer buffer);

void
accelerate_2d(PhysicsObject2D *object, v2 acc);
