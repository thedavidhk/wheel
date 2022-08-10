#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include "handmade.h"


double timeDiff(timespec start, timespec end)
{
    long nano;
	if ((end.tv_nsec-start.tv_nsec)<0)
		nano = 1000000000+end.tv_nsec-start.tv_nsec;
    else
        nano = end.tv_nsec-start.tv_nsec;
    return (double)nano / 1000000000.0f;
}


int main(int argc, char **argv) {

    Display *display = XOpenDisplay(0);

    if (!display) {
        printf("No display available.\n");
        exit(1);
    }


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
    XSelectInput(display, window, KeyPressMask | KeyReleaseMask);

    // Show window
    XMapWindow(display, window);
    XFlush(display);

    // Drawing pixels

    PixelBuffer buffer = {};
    buffer.width = WIN_WIDTH;
    buffer.height = WIN_HEIGHT;
    buffer.bytes_per_pixel = 4;
    buffer.data = (unsigned int*)malloc(buffer.width * buffer.height * buffer.bytes_per_pixel);


#if 1
    // TODO: Use shared memory? (see below)
    XImage *xWindowBuffer = XCreateImage(display, visinfo.visual,
            visinfo.depth, ZPixmap, 0, (char *)buffer.data, buffer.width, buffer.height, buffer.bytes_per_pixel * 4, 0);
#else
    XShmSegmentInfo shminfo;
    XImage *xWindowBuffer = XShmCreateImage(display, visinfo.visual,
            visinfo.depth, ZPixmap, 0, &shminfo, WIN_WIDTH, WIN_HEIGHT);

    shminfo.shmid = shmget (IPC_PRIVATE,
            xWindowBuffer->bytes_per_line * xWindowBuffer->height, IPC_CREAT|0777);
    shminfo.readOnly = 0;
    shminfo.shmaddr = xWindowBuffer->data = (char *)shmat(shminfo.shmid, 0, 0);
    Status XShmAttach(display, shminfo);
#endif

    GC defaultGC = DefaultGC(display, defaultScreen);

    // Graceful window close

    Atom WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if (!XSetWMProtocols(display, window, &WM_DELETE_WINDOW, 1)) {
        printf("Couldn't register WM_DELETE_WINDOW property.\n");
    }

    GameMemory mem = initializeGame(megabytes(64));

    GameInput input = {};

    int windowOpen = 1;
    double d_t_work, d_t_frame = 0;
    while(windowOpen) {
        timespec t_start, t_end;
        clock_gettime(CLOCK_REALTIME, &t_start);

        // Events
        XEvent ev = {};
        while(XPending(display) > 0) {
            XNextEvent(display, &ev);
            switch(ev.type) {
                case DestroyNotify: {
                                        // NOTE: event-window needs to be checked if there are multiple windows.
                                        windowOpen = 0;
                                    } break;
                case ClientMessage: {
                                        XClientMessageEvent *e = (XClientMessageEvent*) &ev;
                                        if ((Atom)e->data.l[0] == WM_DELETE_WINDOW) {
                                            XDestroyWindow(display, window);
                                            windowOpen = 0;
                                        }
                                    } break;
                case KeyPress: {
                                   XKeyPressedEvent *e = (XKeyPressedEvent*) &ev;
                                   if(e->keycode == XKeysymToKeycode(display, XK_Left))
                                       input.left = true;
                                   if(e->keycode == XKeysymToKeycode(display, XK_Right))
                                       input.right = true;
                                   if(e->keycode == XKeysymToKeycode(display, XK_Up))
                                       input.up = true;
                                   if(e->keycode == XKeysymToKeycode(display, XK_Down))
                                       input.down = true;
                                   if(e->keycode == XKeysymToKeycode(display, XK_space))
                                       input.pause = true;
                                   if(e->keycode == XKeysymToKeycode(display, XK_A))
                                       input.fwd = true;
                               } break;
                case KeyRelease: {
                                   XKeyReleasedEvent *e = (XKeyReleasedEvent*) &ev;
                                   if(e->keycode == XKeysymToKeycode(display, XK_Left))
                                       input.left = false;
                                   if(e->keycode == XKeysymToKeycode(display, XK_Right))
                                       input.right = false;
                                   if(e->keycode == XKeysymToKeycode(display, XK_Up))
                                       input.up = false;
                                   if(e->keycode == XKeysymToKeycode(display, XK_Down))
                                       input.down = false;
                                   if(e->keycode == XKeysymToKeycode(display, XK_space))
                                       input.pause = false;
                                   if(e->keycode == XKeysymToKeycode(display, XK_A))
                                       input.fwd = false;
                               } break;
                default:
                                 XFlush(display);
                                 break;
            }
        }

        gameUpdateAndRender(d_t_frame, mem, &input, buffer);

#if 1
        // TODO: use shared memory (see below)?
        XPutImage(display, window, defaultGC, xWindowBuffer, 0, 0, 0, 0, buffer.width, buffer.height);
#else
        XShmPutImage(display, window, defaultGC, xWindowBuffer, 0, 0, 0, 0, buffer.width, buffer.height, 0);
#endif

        clock_gettime(CLOCK_REALTIME, &t_end);
        d_t_work = timeDiff(t_start, t_end);

        while(d_t_work < 1.0f/FRAME_RATE) {
            clock_gettime(CLOCK_REALTIME, &t_end);
            d_t_work = timeDiff(t_start, t_end);
        }
        clock_gettime(CLOCK_REALTIME, &t_end);
        d_t_frame = timeDiff(t_start, t_end);
    }
    return 0;
}
