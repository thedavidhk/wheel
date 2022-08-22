#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/XKBlib.h>

#define SHARED_MEM_SUPORT
#ifdef SHARED_MEM_SUPORT
#include <sys/ipc.h>
#include <sys/shm.h>
#include <X11/extensions/XShm.h>
#endif

#include "wheel.h"

static KeyBoardInput
get_key(int xkc, Display* disp, unsigned int state) {
    int keysym = XkbKeycodeToKeysym(disp, xkc, 0, state);
    switch (keysym) {
    case XK_a:
        return KEY_A;
    case XK_b:
        return KEY_B;
    case XK_c:
        return KEY_C;
    case XK_d:
        return KEY_D;
    case XK_e:
        return KEY_E;
    case XK_f:
        return KEY_F;
    case XK_g:
        return KEY_G;
    case XK_h:
        return KEY_H;
    case XK_i:
        return KEY_I;
    case XK_j:
        return KEY_J;
    case XK_k:
        return KEY_K;
    case XK_l:
        return KEY_L;
    case XK_m:
        return KEY_M;
    case XK_n:
        return KEY_N;
    case XK_o:
        return KEY_O;
    case XK_p:
        return KEY_P;
    case XK_q:
        return KEY_Q;
    case XK_r:
        return KEY_R;
    case XK_s:
        return KEY_S;
    case XK_t:
        return KEY_T;
    case XK_u:
        return KEY_U;
    case XK_v:
        return KEY_V;
    case XK_w:
        return KEY_W;
    case XK_x:
        return KEY_X;
    case XK_y:
        return KEY_Y;
    case XK_z:
        return KEY_Z;
    case XK_0:
        return KEY_0;
    case XK_1:
        return KEY_1;
    case XK_2:
        return KEY_2;
    case XK_3:
        return KEY_3;
    case XK_4:
        return KEY_4;
    case XK_5:
        return KEY_5;
    case XK_6:
        return KEY_6;
    case XK_7:
        return KEY_7;
    case XK_8:
        return KEY_8;
    case XK_9:
        return KEY_9;
    case XK_Escape:
        return KEY_ESC;
    case XK_space:
        return KEY_SPACE;
    case XK_Up:
        return KEY_UP;
    case XK_Down:
        return KEY_DOWN;
    case XK_Left:
        return KEY_LEFT;
    case XK_Right:
        return KEY_RIGHT;
    default:
        return KEY_NULL;
    };
}

static MouseButton
get_mouse_button(unsigned int xbutton) {
    switch (xbutton) {
        case Button1:
            return BUTTON_1;
        case Button2:
            return BUTTON_2;
        case Button3:
            return BUTTON_3;
        case Button4:
            return BUTTON_4;
        case Button5:
            return BUTTON_5;
        default:
            return BUTTON_NULL;
    }
}

static InputType
get_input_type(int type) {
    switch (type) {
        case (KeyRelease):
        case (ButtonRelease):
            return IT_RELEASED;
        case (KeyPress):
        case (ButtonPress):
            return IT_PRESSED;
        default:
            return IT_NULL;
    }
}

static double
timeDiff(timespec start, timespec end)
{
    long nano;
    if ((end.tv_nsec-start.tv_nsec)<0)
        nano = 1000000000+end.tv_nsec-start.tv_nsec;
    else
        nano = end.tv_nsec-start.tv_nsec;
    return (double)nano / 1000000000.0f;
}

static XImage *
#ifdef SHARED_MEM_SUPORT
ximage_create(Display *display, XVisualInfo *visinfo, XShmSegmentInfo* shminfo, Framebuffer *fb, uint32 width, uint32 height) {
    XImage *image = XShmCreateImage(display, visinfo->visual,
            visinfo->depth, ZPixmap, 0, shminfo, width, height);
    shminfo->shmid = shmget(IPC_PRIVATE, image->bytes_per_line *
            image->height, IPC_CREAT|0777);
    shminfo->shmaddr = image->data = (char *)shmat(shminfo->shmid, 0, 0);
    shminfo->readOnly = false;
    if(!XShmAttach(display, shminfo)) {
        printf("Could not attach shared memory to X-Server.\n");
        exit(1);
    }
    fb->data = (unsigned int *)image->data;
    return image;
}
#else
ximage_create(Display *display, XVisualInfo *visinfo, Framebuffer *fb, uint32 width, uint32 height) {
    fb->data = (unsigned int*)malloc(fb->width * fb->height * fb->bytes_per_pixel);
    XImage *image = XCreateImage(display, visinfo->visual,
            visinfo->depth, ZPixmap, 0, (char *)fb->data, fb->width, fb->height, fb->bytes_per_pixel * 4, 0);
    return image;
}
#endif


int main(int argc, char **argv) {

    Display *display = XOpenDisplay(0);

    if (!display) {
        printf("No display available.\n");
        exit(1);
    }

#ifdef SHARED_MEM_SUPORT
    if (!XShmQueryExtension(display)) {
        printf("Shared memory not supported on system.");
        exit(1);
    }
#endif

    int root = DefaultRootWindow(display);
    int defaultScreen = DefaultScreen(display);

    int screenBitDepth = 24;
    XVisualInfo visinfo = {};
    if(!XMatchVisualInfo(display, defaultScreen, screenBitDepth, TrueColor, &visinfo)) {
        printf("No matching visual info.\n");
        exit(1);
    }
    XSetWindowAttributes windowAttr;
    windowAttr.background_pixel = 0;
    windowAttr.colormap = XCreateColormap(display, root, visinfo.visual, AllocNone);
    windowAttr.event_mask = StructureNotifyMask;
    unsigned long attributeMask = CWBackPixel | CWColormap | CWEventMask;

    Window window = XCreateWindow(display, root,
            0,0,
            WIN_WIDTH, WIN_HEIGHT, 0,
            visinfo.depth, InputOutput,
            visinfo.visual, attributeMask, &windowAttr);

    if(!window) {
        printf("Window wasn't created properly.\n");
        exit(1);
    }

    XStoreName(display, window, "Hello, World!");

    // Filter events
    XSelectInput(display, window, KeyPressMask | KeyReleaseMask | PointerMotionMask | ButtonPressMask | ButtonReleaseMask);

    // Show window
    XMapWindow(display, window);
    XFlush(display);

    // Drawing pixels

    Framebuffer fb = {};
    fb.width = WIN_WIDTH;
    fb.height = WIN_HEIGHT;
    fb.bytes_per_pixel = 4;


#ifdef SHARED_MEM_SUPORT
    XShmSegmentInfo shminfo_a;
    XImage *ximage_a = ximage_create(display, &visinfo, &shminfo_a, &fb, WIN_WIDTH, WIN_HEIGHT);
    XShmSegmentInfo shminfo_b;
    XImage *ximage_b = ximage_create(display, &visinfo, &shminfo_b, &fb, WIN_WIDTH, WIN_HEIGHT);
#else
    XImage *ximage_a = ximage_create(display, &visinfo, &fb, WIN_WIDTH, WIN_HEIGHT);
    XImage *ximage_b = ximage_create(display, &visinfo, &fb, WIN_WIDTH, WIN_HEIGHT);
#endif

    fb.data = (uint32 *)ximage_a->data;

    GC defaultGC = DefaultGC(display, defaultScreen);

    // Graceful window close

    Atom WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if (!XSetWMProtocols(display, window, &WM_DELETE_WINDOW, 1)) {
        printf("Couldn't register WM_DELETE_WINDOW property.\n");
    }

    AppHandle app = initialize_app();

    int windowOpen = 1;
    double d_t_work, d_t_frame = 0;
    while(windowOpen) {
        timespec t_start, t_end;
        clock_gettime(CLOCK_REALTIME, &t_start);

        // Events
        XEvent ev = {};
        XEvent nev = {};
        KeyBoardInput key;
        MouseButton btn;
        InputType t;
        while(XPending(display) > 0) {
            XNextEvent(display, &ev);
            bool is_repeat = false;
            switch(ev.type) {
                case DestroyNotify:
                    // NOTE: event-window needs to be checked if there are multiple windows.
                    windowOpen = 0;
                    break;
                case ClientMessage:
                    if ((Atom)((XClientMessageEvent *)&ev)->data.l[0] == WM_DELETE_WINDOW) {
                        XDestroyWindow(display, window);
                        windowOpen = 0;
                    }
                    break;
                case KeyPress:
                    key = get_key(ev.xkey.keycode, display, ev.xkey.state);
                    t = get_input_type(ev.type);
                    key_callback(key, t, app);
                    break;
                case KeyRelease:
                    if (XEventsQueued(display, QueuedAfterReading)) {
                        XPeekEvent(display, &nev);
                        if (nev.type == KeyPress && nev.xkey.time == ev.xkey.time && nev.xkey.keycode == ev.xkey.keycode) {
                            is_repeat = true;
                            XNextEvent(display, &ev);
                        }
                    }
                    if (!is_repeat) {
                        key = get_key(ev.xkey.keycode, display, ev.xkey.state);
                        t = get_input_type(ev.type);
                        key_callback(key, t, app);
                    }
                    break;
                case MotionNotify:
                    mouse_move_callback(ev.xmotion.x, ev.xmotion.y, ev.xmotion.state, app);
                    break;
                case ButtonPress:
                case ButtonRelease:
                    t = get_input_type(ev.type);
                    btn = get_mouse_button(ev.xbutton.button);
                    mouse_button_callback(btn, t, ev.xbutton.x, ev.xbutton.y, app);
                default:
                    XFlush(display);
                    break;
            }
        }

        app_update_and_render(d_t_frame, app, fb);

        XImage *read_img = ximage_a;

        if (fb.data == (uint32 *)ximage_a->data) {
            fb.data = (uint32 *)ximage_b->data;
            read_img = ximage_a;
        } else {
            fb.data = (uint32 *)ximage_a->data;
            read_img = ximage_b;
        }

#ifdef SHARED_MEM_SUPORT
        XShmPutImage(display, window, defaultGC, read_img, 0, 0, 0, 0, fb.width,fb.height, 0);
#else
        XPutImage(display, window, defaultGC, read_img, 0, 0, 0, 0, fb.width,fb.height);
#endif

#ifdef FRAME_RATE
        clock_gettime(CLOCK_REALTIME, &t_end);
        d_t_work = timeDiff(t_start, t_end);

        if (d_t_work < 1.0f/FRAME_RATE) {
            // TODO: This does not seem reliable
            usleep((1.0 / FRAME_RATE - d_t_work) * 1000000);
        }

        //while(d_t_work < 1.0f/FRAME_RATE) {
        //    clock_gettime(CLOCK_REALTIME, &t_end);
        //    d_t_work = timeDiff(t_start, t_end);
        //}
#endif

        clock_gettime(CLOCK_REALTIME, &t_end);
        d_t_frame = timeDiff(t_start, t_end);
    }
    return 0;
}
