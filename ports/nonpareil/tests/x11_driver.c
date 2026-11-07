/* SPDX-License-Identifier: MIT
 * Drive a single Nonpareil test display and capture its client window.
 */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Window find_calculator(Display *display)
{
    Window root, parent, *children = NULL, result = None;
    unsigned count = 0, index;

    if (!XQueryTree(display, DefaultRootWindow(display), &root, &parent,
                    &children, &count)) {
        return None;
    }
    for (index = 0; index < count; index++) {
        char *name = NULL;
        if (XFetchName(display, children[index], &name) && name) {
            if (strstr(name, "Calculator")) {
                result = children[index];
            }
            XFree(name);
        }
    }
    XFree(children);
    return result;
}

static unsigned char channel(unsigned long pixel, unsigned long mask)
{
    if (!mask) {
        return 0;
    }
    while (!(mask & 1)) {
        pixel >>= 1;
        mask >>= 1;
    }
    return (pixel & mask) * 255 / mask;
}

static int capture(Display *display, Window window, const char *filename)
{
    XWindowAttributes attributes;
    XImage *image;
    FILE *output;
    int x, y;

    XGetWindowAttributes(display, window, &attributes);
    image = XGetImage(display, window, 0, 0, attributes.width, attributes.height,
                      AllPlanes, ZPixmap);
    output = fopen(filename, "wb");
    if (!image || !output) {
        return 1;
    }
    fprintf(output, "P6\n%d %d\n255\n", attributes.width, attributes.height);
    for (y = 0; y < attributes.height; y++) {
        for (x = 0; x < attributes.width; x++) {
            unsigned long pixel = XGetPixel(image, x, y);
            fputc(channel(pixel, image->red_mask), output);
            fputc(channel(pixel, image->green_mask), output);
            fputc(channel(pixel, image->blue_mask), output);
        }
    }
    XDestroyImage(image);
    return fclose(output) != 0;
}

int main(int argc, char **argv)
{
    Display *display = XOpenDisplay(NULL);
    Window window;
    XEvent event = {0};
    int result = 0;

    if (!display || argc < 2) {
        return 2;
    }
    if (!strcmp(argv[1], "wait")) {
        unsigned attempt;
        for (attempt = 0; attempt < 400; attempt++) {
            XWindowAttributes attributes;
            window = find_calculator(display);
            if (window && XGetWindowAttributes(display, window, &attributes) &&
                attributes.map_state == IsViewable) {
                XCloseDisplay(display);
                return 0;
            }
            usleep(50000);
        }
        fputs("Calculator did not become ready\n", stderr);
        return 1;
    }
    window = find_calculator(display);
    if (!window) {
        fputs("No calculator window\n", stderr);
        return 1;
    }
    if (!strcmp(argv[1], "capture") && argc == 3) {
        result = capture(display, window, argv[2]);
    } else if (!strcmp(argv[1], "click") && argc == 4) {
        event.xbutton.type = ButtonPress;
        event.xbutton.display = display;
        event.xbutton.window = window;
        event.xbutton.root = DefaultRootWindow(display);
        event.xbutton.x = atoi(argv[2]);
        event.xbutton.y = atoi(argv[3]);
        event.xbutton.button = Button1;
        event.xbutton.same_screen = True;
        XSendEvent(display, window, False, ButtonPressMask, &event);
        XFlush(display);
        usleep(180000);
        event.type = ButtonRelease;
        XSendEvent(display, window, False, ButtonReleaseMask, &event);
    } else if ((!strcmp(argv[1], "key") || !strcmp(argv[1], "tap")) && argc == 3) {
        KeySym symbol = XStringToKeysym(argv[2]);
        KeyCode code = XKeysymToKeycode(display, symbol);
        event.xkey.type = KeyPress;
        event.xkey.display = display;
        event.xkey.window = window;
        event.xkey.root = DefaultRootWindow(display);
        event.xkey.keycode = code;
        event.xkey.same_screen = True;
        if (XKeycodeToKeysym(display, code, 1) == symbol) {
            event.xkey.state = ShiftMask;
        }
        XSendEvent(display, window, False, KeyPressMask, &event);
        if (!strcmp(argv[1], "key")) {
            XFlush(display);
            usleep(180000);
        }
        event.type = KeyRelease;
        XSendEvent(display, window, False, KeyReleaseMask, &event);
    } else if (!strcmp(argv[1], "close")) {
        event.xclient.type = ClientMessage;
        event.xclient.window = window;
        event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
        event.xclient.format = 32;
        event.xclient.data.l[0] = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSendEvent(display, window, False, NoEventMask, &event);
    } else {
        result = 2;
    }
    XSync(display, False);
    XCloseDisplay(display);
    return result;
}
