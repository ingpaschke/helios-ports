/* Send ordinary X11 key events to NetHack's largest mapped input widget.
 * This is an external test client; no test hooks are compiled into the game. */
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static Window best;
static unsigned long best_area;
static void find_input(Display *d, Window w)
{
    Window root, parent, *children;
    unsigned int n, i;
    XWindowAttributes a;
    if (XGetWindowAttributes(d, w, &a) && a.map_state == IsViewable &&
        (a.all_event_masks & KeyPressMask) &&
        (unsigned long)a.width * a.height > best_area) {
        best = w;
        best_area = (unsigned long)a.width * a.height;
    }
    if (XQueryTree(d, w, &root, &parent, &children, &n)) {
        for (i = 0; i < n; ++i) find_input(d, children[i]);
        if (children) XFree((char *)children);
    }
}

static int choose_input(Display *d)
{
    Window root, parent, transient, *children, saved;
    unsigned int n, i;
    unsigned long saved_area;
    XClassHint hint;
    XWindowAttributes a;
    int found = 0;
    best = None;
    best_area = 0;
    root = DefaultRootWindow(d);
    if (!XQueryTree(d, root, &root, &parent, &children, &n)) return 0;
    for (i = 0; i < n; ++i) {
        if (XGetClassHint(d, children[i], &hint)) {
            if (hint.res_class && !strcmp(hint.res_class, "NetHack")) {
                /* Prefer the topmost mapped query/dialog over the map.
                 * Xt's exclusive popup grab would discard map-directed keys. */
                saved = best;
                saved_area = best_area;
                if (XGetWindowAttributes(d, children[i], &a) &&
                    a.map_state == IsViewable &&
                    XGetTransientForHint(d, children[i], &transient)) {
                    best = None;
                    best_area = 0;
                }
                find_input(d, children[i]);
                if (!best) { best = saved; best_area = saved_area; }
                found = 1;
            }
            if (hint.res_name) XFree(hint.res_name);
            if (hint.res_class) XFree(hint.res_class);
        }
    }
    if (children) XFree((char *)children);
    return found && best != None;
}

int main(int argc, char **argv)
{
    Display *d = XOpenDisplay(NULL);
    unsigned int i;
    XEvent e;
    KeySym sym;
    if (!d) { fputs("Cannot open display\n", stderr); return 1; }
    if (!choose_input(d)) { fputs("NetHack input window missing\n", stderr); return 3; }
    printf("NetHack input window 0x%lx area %lu\n", best, best_area);
    for (i = 1; i < argc; ++i) {
        if (!choose_input(d)) return 3;
        sym = XStringToKeysym(argv[i]);
        if (!sym) { fprintf(stderr, "Unknown keysym %s\n", argv[i]); return 4; }
        memset(&e, 0, sizeof(e));
        e.xkey.type = KeyPress;
        e.xkey.display = d;
        e.xkey.window = best;
        e.xkey.root = DefaultRootWindow(d);
        e.xkey.time = CurrentTime;
        e.xkey.same_screen = True;
        e.xkey.keycode = XKeysymToKeycode(d, sym);
        if (sym >= 'A' && sym <= 'Z') e.xkey.state = ShiftMask;
        if (!e.xkey.keycode || !XSendEvent(d, best, False, KeyPressMask, &e)) return 5;
        XSync(d, False);
        sleep(1);
    }
    XCloseDisplay(d);
    return 0;
}
