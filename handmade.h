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


struct GameState {
    int player_speed;
    int player_size;
    int player_x;
    int player_y;
};


struct PixelBuffer {
    char *data;
    int width, height, bytes_per_pixel;
};

void
drawRectangle(PixelBuffer buffer, int startX, int startY, int endX, int endY, unsigned int color);

void
clearScreen(PixelBuffer buffer, unsigned int color);

GameMemory
initializeGame(unsigned long long mem_size);

void
gameUpdateAndRender(double d_t, GameMemory mem, GameInput *input, PixelBuffer buffer);
