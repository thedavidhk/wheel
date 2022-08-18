#ifndef WHEEL_H
#define WHEEL_H

#define int8 char
#define int16 short
#define int32 int
#define int64 long long

#define uint8 unsigned char
#define uint16 unsigned short
#define uint32 unsigned int
#define uint64 unsigned long long

#define f32 float
#define f64 double

#define FRAME_RATE 60
#define SIM_RATE 120
#define WIN_WIDTH 800
#define WIN_HEIGHT 600

#define kilobytes(value) ((value) * 1024)
#define megabytes(value) (kilobytes(value) * 1024)
#define gigabytes(value) (megabytes(value) * 1024)

#define assert(b) do { if (!(b)) {printf("Assertion failed: %s in line %d of file %s\n", __STRING(b), __LINE__, __FILE__); exit(1);} } while (0)

typedef void *AppHandle;

enum InputType {
    IT_NULL,
    IT_PRESSED,
    IT_RELEASED
};

enum KeyBoardInput {
    KEY_NULL,
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,
    KEY_0,
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_SPACE,
    KEY_ESC
};

enum MouseButton {
    BUTTON_NULL,
    BUTTON_1,
    BUTTON_2,
    BUTTON_3,
    BUTTON_4,
    BUTTON_5
};

struct GameInput {
    bool up, down, left, right, pause, fwd;
};

struct Framebuffer {
    unsigned int *data;
    int width, height, bytes_per_pixel;
};

AppHandle
initialize_app();

void
app_update_and_render(double d_t, AppHandle game, Framebuffer buffer);

void
key_callback(KeyBoardInput key, InputType t, AppHandle game);

void
mouse_button_callback(MouseButton, InputType t, int x, int y, AppHandle game);

void
mouse_move_callback(int x, int y, uint32 mask, AppHandle game);

#endif
