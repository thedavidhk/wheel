#pragma once

#define FRAME_RATE 60
#define WIN_WIDTH 800
#define WIN_HEIGHT 600

#define kilobytes(value) ((value) * 1024)
#define megabytes(value) (kilobytes(value) * 1024)
#define gigabytes(value) (megabytes(value) * 1024)

struct GameMemory {
    unsigned long long size;
    void *data;
    void *free;
};

struct GameInput {
    bool up, down, left, right;
};

struct PixelBuffer {
    char *data;
    int width, height, bytes_per_pixel;
};

GameMemory
initializeGame(unsigned long long mem_size);

void
gameUpdateAndRender(double d_t, GameMemory mem, GameInput *input, PixelBuffer buffer);
