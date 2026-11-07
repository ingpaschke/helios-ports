/* Exercise the renderer against a real X server without moving the pointer. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "desktop.h"

static unsigned int arcs, logos, clocks;
static int fail_pixmap;
static time_t clock_time = 1700000000;

int __real_XDrawArc(Display *, Drawable, GC, int, int, unsigned int,
                    unsigned int, int, int);
int __real_XFillRectangles(Display *, Drawable, GC, XRectangle *, int);
int __real_XDrawString(Display *, Drawable, GC, int, int, const char *, int);
Pixmap __real_XCreatePixmap(Display *, Drawable, unsigned int,
                            unsigned int, unsigned int);

int __wrap_XDrawArc(Display *d, Drawable target, GC gc, int x, int y,
                    unsigned int w, unsigned int h, int a, int b)
{
    arcs++;
    return __real_XDrawArc(d, target, gc, x, y, w, h, a, b);
}

int __wrap_XFillRectangles(Display *d, Drawable target, GC gc,
                           XRectangle *rectangles, int count)
{
    logos++;
    return __real_XFillRectangles(d, target, gc, rectangles, count);
}

int __wrap_XDrawString(Display *d, Drawable target, GC gc, int x, int y,
                       const char *text, int length)
{
    clocks++;
    return __real_XDrawString(d, target, gc, x, y, text, length);
}

Pixmap __wrap_XCreatePixmap(Display *d, Drawable target, unsigned int w,
                            unsigned int h, unsigned int depth)
{
    if ((fail_pixmap == 1 && h == 64) ||
        (fail_pixmap == 2 && h != 64 && h != 1)) {
        /* A real asynchronous allocation error exercises the fallback. */
        depth = 255;
    }
    return __real_XCreatePixmap(d, target, w, h, depth);
}

time_t __wrap_time(time_t *result)
{
    if (result) {
        *result = clock_time;
    }
    return clock_time;
}

static void drain(Display *display)
{
    XEvent event;
    int pass;

    for (pass = 0; pass < 3; pass++) {
        XSync(display, False);
        while (XPending(display)) {
            XNextEvent(display, &event);
            helios_desktop_event(&event);
        }
    }
}

static Window header_window(Display *display)
{
    Window header;
    Atom type, owner = XInternAtom(display, "_HELIOS_BACKDROP_OWNER", False);
    int format;
    unsigned long count, left;
    unsigned char *data = NULL;

    assert(XGetWindowProperty(display, DefaultRootWindow(display), owner,
                               0, 1, False, XA_WINDOW, &type, &format,
                               &count, &left, &data) == Success);
    assert(type == XA_WINDOW && format == 32 && count == 1 && data);
    header = *(unsigned long *)data;
    XFree(data);
    assert(header != None);
    return header;
}

static void expose(Window window, int x, int y, int w, int h, int count)
{
    XEvent event;

    memset(&event, 0, sizeof(event));
    event.type = Expose;
    event.xexpose.window = window;
    event.xexpose.x = x;
    event.xexpose.y = y;
    event.xexpose.width = w;
    event.xexpose.height = h;
    event.xexpose.count = count;
    helios_desktop_event(&event);
}

static void reset_counts(void)
{
    arcs = logos = clocks = 0;
}

static void compare_images(XImage *before, XImage *after)
{
    int x, y;

    assert(before && after);
    for (y = 0; y < before->height; y++) {
        for (x = 0; x < before->width; x++) {
            if (XGetPixel(before, x, y) != XGetPixel(after, x, y)) {
                fprintf(stderr, "Background damaged at %d,%d\n", x, y);
                abort();
            }
        }
    }
}

static void move_cover(Display *display)
{
    Window root = DefaultRootWindow(display);
    XSetWindowAttributes attributes;
    Window cover;
    XImage *before, *after;
    int screen = DefaultScreen(display);
    int width = DisplayWidth(display, screen);
    int height = DisplayHeight(display, screen);
    int i;

    before = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
    attributes.override_redirect = True;
    attributes.background_pixel = WhitePixel(display, screen);
    cover = XCreateWindow(display, root, 0, 0, 180, 130, 0, CopyFromParent,
                           InputOutput, CopyFromParent,
                           CWOverrideRedirect | CWBackPixel, &attributes);
    XMapRaised(display, cover);
    drain(display);
    XMoveWindow(display, cover, width - 180, 0);
    drain(display);
    XMoveWindow(display, cover, width - 180, height - 130);
    drain(display);
    for (i = 0; i < 40; i++) {
        XMoveWindow(display, cover, (i * 53) % (width - 180),
                     (i * 37) % (height - 130));
        drain(display);
    }
    XDestroyWindow(display, cover);
    drain(display);
    after = XGetImage(display, root, 0, 0, width, height, AllPlanes, ZPixmap);
    compare_images(before, after);
    XDestroyImage(before);
    XDestroyImage(after);
}

static void exercise(Display *display, int cache)
{
    Window header;
    XWindowAttributes attributes;
    int wanted_cache = cache;

    assert(helios_desktop_open(display, NULL, cache));
    drain(display);
    header = header_window(display);
    assert(XGetWindowAttributes(display, header, &attributes));
    if (fail_pixmap) {
        cache = 0;
    }
    assert(!!(attributes.your_event_mask & ExposureMask) == !cache);
    reset_counts();
    expose(DefaultRootWindow(display), 10, 100, 12, 12, 0);
    expose(header, 250, 0, 10, 10, 0);
    assert(!arcs && !logos && !clocks);
    if (!cache) {
        /* Both rectangles in a batch must contribute to the clip region. */
        expose(header, 22, 11, 100, 40, 1);
        assert(!logos && !clocks);
        expose(header, attributes.width - 320, 16, 296, 30, 0);
        assert(logos && clocks == 1);
    }
    reset_counts();
    move_cover(display);
    if (cache) {
        assert(!arcs && !logos && !clocks);
    } else {
        assert(arcs && logos && clocks);
    }
    printf("%s: 40 moves, %u arc / %u logo / %u clock calls; pixels intact\n",
            cache ? "cached" : "low-memory", arcs, logos, clocks);
    reset_counts();
    helios_desktop_tick();
    assert(!arcs && !logos && !clocks);
    clock_time += 60;
    helios_desktop_tick();
    assert(!arcs && !logos && clocks == 1);
    helios_desktop_close();
    drain(display);
    assert(helios_desktop_tick() == -1);
    printf("%s: minute tick and cleanup passed%s\n",
            cache ? "cached" : "low-memory",
            wanted_cache && fail_pixmap ? " (allocation failure fallback)" : "");
}

static void replacement(Display *display)
{
    Window replacement = None;
    Atom owner = XInternAtom(display, "_HELIOS_BACKDROP_OWNER", False);

    assert(helios_desktop_open(display, NULL, 1));
    drain(display);
    XChangeProperty(display, DefaultRootWindow(display), owner, XA_WINDOW, 32,
                    PropModeReplace, (unsigned char *)&replacement, 1);
    drain(display);
    assert(helios_desktop_tick() == -1);
    puts("ownership replacement: PASS");
}

int main(int argc, char **argv)
{
    Display *display = XOpenDisplay(NULL);

    assert(display);
    if (argc == 2 && !strcmp(argv[1], "--external")) {
        XWindowAttributes attributes;
        Window header = header_window(display);

        assert(XGetWindowAttributes(display, header, &attributes));
        assert(!(attributes.all_event_masks & ExposureMask));
        move_cover(display);
        XCloseDisplay(display);
        puts("external cached renderer: pixels intact after window moves");
        return 0;
    }
    exercise(display, 0);
    exercise(display, 1);
    fail_pixmap = 1;
    exercise(display, 1);
    fail_pixmap = 2;
    exercise(display, 1);
    fail_pixmap = 0;
    replacement(display);
    XCloseDisplay(display);
    puts("desktop: PASS");
    return 0;
}
