/* helios-place NAME X Y: wait for a top-level window called NAME (WM_NAME,
 * or WM_CLASS name or class) and ask the window manager to move its frame
 * to X,Y. A negative X or "-0" measures from the right edge of the screen.
 * For clients such as the vintage xlights that take no -geometry option. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslib.h>

static int named(Display *d, Window w, const char *name)
{
    char *title = 0;
    XClassHint hint;
    int match = 0;
    if (XFetchName(d, w, &title) && title) {
        match = !strcmp(title, name);
        XFree(title);
    }
    if (!match && XGetClassHint(d, w, &hint)) {
        match = (hint.res_name && !strcmp(hint.res_name, name)) ||
                (hint.res_class && !strcmp(hint.res_class, name));
        if (hint.res_name) XFree(hint.res_name);
        if (hint.res_class) XFree(hint.res_class);
    }
    return match;
}

/* Search a frame's descendants, which a reparenting manager adds. */
static Window find(Display *d, Window w, const char *name, int depth)
{
    Window root, parent, *children, found = 0;
    unsigned int n, i;
    if (named(d, w, name)) return w;
    if (!depth || !XQueryTree(d, w, &root, &parent, &children, &n)) return 0;
    for (i = 0; i < n && !found; i++) found = find(d, children[i], name, depth - 1);
    if (children) XFree(children);
    return found;
}

int main(int argc, char **argv)
{
    Display *d;
    Window root, top, parent, *frames, client = 0, frame = 0, junk;
    unsigned int n, i, fw, fh, border, depth;
    int fx, fy, cx, cy, x, y, tries, right;
    if (argc != 4) {
        fprintf(stderr, "usage: %s NAME X Y\n", argv[0]);
        return 2;
    }
    if (!(d = XOpenDisplay(0))) return 1;
    root = DefaultRootWindow(d);
    right = argv[2][0] == '-';
    x = abs(atoi(argv[2]));
    y = atoi(argv[3]);
    for (tries = 0; tries < 120 && !client; tries++) {
        if (tries) Delay(250000);
        if (!XQueryTree(d, root, &top, &parent, &frames, &n)) continue;
        for (i = n; i-- > 0 && !client; ) {
            client = find(d, frames[i], argv[1], 3);
            frame = frames[i];
        }
        if (frames) XFree(frames);
    }
    if (!client) {
        fprintf(stderr, "%s: no window %s\n", argv[0], argv[1]);
        XCloseDisplay(d);
        return 1;
    }
    /* Move the frame: keep the client's offset within it, which the
     * manager subtracts again from the requested client position. */
    XGetGeometry(d, frame, &junk, &fx, &fy, &fw, &fh, &border, &depth);
    XTranslateCoordinates(d, client, root, 0, 0, &cx, &cy, &junk);
    if (right) x = DisplayWidth(d, DefaultScreen(d)) - (int)(fw + 2 * border) - x;
    XMoveWindow(d, client, x + cx - fx, y + cy - fy);
    XSync(d, False);
    XCloseDisplay(d);
    return 0;
}
