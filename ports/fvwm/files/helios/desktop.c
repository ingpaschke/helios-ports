/* The low-memory renderer repaints exposed areas only. Optional pixmaps let
 * the X server restore the header and pattern without contacting Helios. */
#include "desktop.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <string.h>
#include <time.h>
#include "helios-logo-mask.h"

#define HEADER_HEIGHT 64
#define LOGO_X 22
#define LOGO_Y 11
#define ARC_COUNT 5
#define PATTERN_WIDTH 518
#define PATTERN_HEIGHT 548

static Display *display;
static Window root, header, pattern_window;
static Pixmap token, header_pixmap, pattern_pixmap;
static Region header_damage, root_damage;
static Atom owner_property;
static GC gc;
static XFontStruct *font;
static int own_font, width, height, active, cached;
static int pattern_x, pattern_y;
static unsigned long colors[24];
static int color_count;
static unsigned long background, header_color, ink, pattern, shades[16];
static char stamp[80];

static unsigned long remember_color(XColor *color)
{
    Colormap colormap = DefaultColormap(display, DefaultScreen(display));

    if (XAllocColor(display, colormap, color)) {
        colors[color_count++] = color->pixel;
        return color->pixel;
    }
    return BlackPixel(display, DefaultScreen(display));
}

static unsigned long named_color(const char *name)
{
    XColor color;
    Colormap colormap = DefaultColormap(display, DefaultScreen(display));

    if (!XParseColor(display, colormap, name, &color)) {
        return BlackPixel(display, DefaultScreen(display));
    }
    return remember_color(&color);
}

static int intersects(Region damage, int x, int y, int w, int h)
{
    return !damage || XRectInRegion(damage, x, y, w, h) != RectangleOut;
}

static void paint_logo(Drawable target)
{
    XRectangle spans[64];
    int level, x, y, start, count;

    for (level = 1; level <= 15; level++) {
        XSetForeground(display, gc, shades[level]);
        count = 0;
        for (y = 0; y < logo_height; y++) {
            for (x = 0; x < logo_width;) {
                if (logo_alpha[y * logo_width + x] != level) {
                    x++;
                    continue;
                }
                start = x++;
                while (x < logo_width && logo_alpha[y * logo_width + x] == level) {
                    x++;
                }
                spans[count].x = LOGO_X + start;
                spans[count].y = LOGO_Y + y;
                spans[count].width = x - start;
                spans[count].height = 1;
                count++;
                if (count == 64) {
                    XFillRectangles(display, target, gc, spans, count);
                    count = 0;
                }
            }
        }
        if (count) {
            XFillRectangles(display, target, gc, spans, count);
        }
    }
}

static void paint_pattern(Drawable target, int origin_x, int origin_y, Region damage)
{
    int i, x, y, diameter;

    /* Most root exposures are nowhere near the decoration. */
    if (!intersects(damage, width - PATTERN_WIDTH, height - PATTERN_HEIGHT,
                    PATTERN_WIDTH, PATTERN_HEIGHT)) {
        return;
    }
    XSetForeground(display, gc, pattern);
    for (i = 0; i < ARC_COUNT; i++) {
        x = width - 390 - i * 32;
        y = height - 420 - i * 32;
        diameter = 300 + i * 64;
        if (intersects(damage, x - 1, y - 1, diameter + 3, diameter + 3)) {
            XDrawArc(display, target, gc, x - origin_x, y - origin_y,
                     diameter, diameter, 0, 360 * 64);
        }
    }
}

static void paint_clock(Drawable target)
{
    int length, pixels;

    if (width <= 440) {
        return;
    }
    length = strlen(stamp);
    pixels = length * 6;
    if (font) {
        pixels = XTextWidth(font, stamp, length);
    }
    XSetForeground(display, gc, header_color);
    XFillRectangle(display, target, gc, width - 320, 16, 296, 30);
    XSetForeground(display, gc, ink);
    XDrawString(display, target, gc, width - 28 - pixels, 36, stamp, length);
}

static void paint_header(Drawable target, Region damage)
{
    XRectangle bounds;

    if (damage) {
        XClipBox(damage, &bounds);
    } else {
        bounds.x = 0;
        bounds.y = 0;
        bounds.width = width;
        bounds.height = HEADER_HEIGHT;
    }
    XSetForeground(display, gc, header_color);
    XFillRectangle(display, target, gc, bounds.x, bounds.y,
                   bounds.width, bounds.height);
    if (intersects(damage, LOGO_X, LOGO_Y, logo_width, logo_height)) {
        paint_logo(target);
    }
    if (width > 440 && intersects(damage, width - 320, 16, 296, 30)) {
        paint_clock(target);
    }
}

/* Pixmap allocation fails asynchronously. Limit this error handler to the
 * allocation request, so a small X server can fall back to clipped painting. */
static int pixmap_failed;

static int pixmap_error(Display *d, XErrorEvent *error)
{
    (void)d;
    (void)error;
    pixmap_failed = 1;
    return 0;
}

static Pixmap allocate_pixmap(int w, int h)
{
    int (*previous_handler)(Display *, XErrorEvent *);
    Pixmap pixmap;

    XSync(display, False);
    pixmap_failed = 0;
    previous_handler = XSetErrorHandler(pixmap_error);
    pixmap = XCreatePixmap(display, root, w, h,
                           DefaultDepth(display, DefaultScreen(display)));
    XSync(display, False);
    XSetErrorHandler(previous_handler);
    if (pixmap_failed) {
        return None;
    }
    return pixmap;
}

static int create_cache(void)
{
    int w, h;

    pattern_x = width - PATTERN_WIDTH;
    if (pattern_x < 0) {
        pattern_x = 0;
    }
    pattern_y = height - PATTERN_HEIGHT;
    if (pattern_y < HEADER_HEIGHT) {
        pattern_y = HEADER_HEIGHT;
    }
    w = width - pattern_x;
    h = height - pattern_y;
    header_pixmap = allocate_pixmap(width, HEADER_HEIGHT);
    if (!header_pixmap) {
        return 0;
    }
    if (h > 0) {
        pattern_pixmap = allocate_pixmap(w, h);
        if (!pattern_pixmap) {
            XFreePixmap(display, header_pixmap);
            header_pixmap = None;
            return 0;
        }
        XSetForeground(display, gc, background);
        XFillRectangle(display, pattern_pixmap, gc, 0, 0, w, h);
        paint_pattern(pattern_pixmap, pattern_x, pattern_y, NULL);
    }
    return 1;
}

static Window create_window(int x, int y, int w, int h,
                            unsigned long color, Pixmap pixmap)
{
    XSetWindowAttributes attributes;
    unsigned long mask = CWOverrideRedirect | CWEventMask;

    attributes.override_redirect = True;
    attributes.event_mask = ExposureMask;
    attributes.background_pixel = color;
    mask |= CWBackPixel;
    if (pixmap) {
        attributes.event_mask = 0;
        attributes.background_pixmap = pixmap;
        mask &= ~CWBackPixel;
        mask |= CWBackPixmap;
    }
    return XCreateWindow(display, root, x, y, w, h, 0, CopyFromParent,
                         InputOutput, CopyFromParent, mask, &attributes);
}

void helios_desktop_close(void)
{
    if (!display) {
        return;
    }
    active = 0;
    if (header) {
        XDestroyWindow(display, header);
    }
    if (pattern_window) {
        XDestroyWindow(display, pattern_window);
    }
    if (header_pixmap) {
        XFreePixmap(display, header_pixmap);
    }
    if (pattern_pixmap) {
        XFreePixmap(display, pattern_pixmap);
    }
    if (token) {
        XFreePixmap(display, token);
    }
    if (header_damage) {
        XDestroyRegion(header_damage);
    }
    if (root_damage) {
        XDestroyRegion(root_damage);
    }
    if (gc) {
        XFreeGC(display, gc);
    }
    if (font && own_font) {
        XFreeFont(display, font);
    }
    if (color_count) {
        XFreeColors(display, DefaultColormap(display, DefaultScreen(display)),
                    colors, color_count, 0);
    }
    display = NULL;
    header = pattern_window = None;
    header_pixmap = pattern_pixmap = token = None;
    header_damage = root_damage = NULL;
    gc = NULL;
    font = NULL;
    color_count = 0;
    cached = 0;
}

int helios_desktop_open(Display *d, XFontStruct *existing_font, int use_cache)
{
    XWindowAttributes root_attributes;
    XColor color;
    Atom property;
    int level;

    helios_desktop_close();
    display = d;
    root = DefaultRootWindow(d);
    width = DisplayWidth(d, DefaultScreen(d));
    height = DisplayHeight(d, DefaultScreen(d));
    background = named_color("#304e57");
    header_color = named_color("#29434c");
    ink = named_color("#a9c0bf");
    pattern = named_color("#3b5c63");
    for (level = 1; level <= 15; level++) {
        color.red = (41 + (237 - 41) * level / 15) * 257;
        color.green = (67 + (232 - 67) * level / 15) * 257;
        color.blue = (76 + (216 - 76) * level / 15) * 257;
        shades[level] = remember_color(&color);
    }
    gc = XCreateGC(d, root, 0, NULL);
    own_font = existing_font == NULL;
    font = existing_font;
    if (!font) {
        font = XLoadQueryFont(d, "fixed");
    }
    if (font) {
        XSetFont(d, gc, font->fid);
    }
    if (use_cache) {
        cached = create_cache();
    }
    header = create_window(0, 0, width, HEADER_HEIGHT,
                            header_color, header_pixmap);
    XStoreName(d, header, "Helios-Backdrop");
    header_damage = XCreateRegion();
    root_damage = XCreateRegion();
    if (!header_damage || !root_damage) {
        helios_desktop_close();
        return 0;
    }
    XGetWindowAttributes(d, root, &root_attributes);
    if (!cached) {
        root_attributes.your_event_mask |= ExposureMask;
    }
    XSelectInput(d, root, root_attributes.your_event_mask | PropertyChangeMask);
    XSetWindowBackground(d, root, background);
    XClearWindow(d, root);
    if (pattern_pixmap) {
        pattern_window = create_window(pattern_x, pattern_y,
                                       width - pattern_x, height - pattern_y,
                                       background, pattern_pixmap);
        XStoreName(d, pattern_window, "Helios-Backdrop-Pattern");
        XMapWindow(d, pattern_window);
        XLowerWindow(d, pattern_window);
    }
    active = 1;
    stamp[0] = 0;
    helios_desktop_tick();
    if (cached) {
        paint_header(header_pixmap, NULL);
    } else {
        paint_pattern(root, 0, 0, NULL);
        paint_header(header, NULL);
    }
    XMapWindow(d, header);
    XLowerWindow(d, header);
    owner_property = XInternAtom(d, "_HELIOS_BACKDROP_OWNER", False);
    XChangeProperty(d, root, owner_property, XA_WINDOW, 32, PropModeReplace,
                    (unsigned char *)&header, 1);
    /* Previous renderers close normally on ownership transfer. */
    token = XCreatePixmap(d, root, 1, 1, 1);
    property = XInternAtom(d, "_XSETROOT_ID", False);
    XChangeProperty(d, root, property, XA_PIXMAP, 32, PropModeReplace,
                    (unsigned char *)&token, 1);
    XFlush(d);
    return 1;
}

static void repaint_exposure(XExposeEvent *exposure, Region damage, int is_header)
{
    XRectangle rectangle;

    rectangle.x = exposure->x;
    rectangle.y = exposure->y;
    rectangle.width = exposure->width;
    rectangle.height = exposure->height;
    XUnionRectWithRegion(&rectangle, damage, damage);
    if (exposure->count) {
        return;
    }
    XSetRegion(display, gc, damage);
    if (is_header) {
        paint_header(header, damage);
    } else {
        paint_pattern(root, 0, 0, damage);
    }
    XSetClipMask(display, gc, None);
    XSubtractRegion(damage, damage, damage);
    XFlush(display);
}

int helios_desktop_event(XEvent *event)
{
    if (!active) {
        return 0;
    }
    if (event->type == Expose) {
        if (event->xexpose.window == header) {
            if (!cached) {
                repaint_exposure(&event->xexpose, header_damage, 1);
            }
            return 1;
        }
        if (event->xexpose.window == root && !cached) {
            repaint_exposure(&event->xexpose, root_damage, 0);
        }
    } else if (event->type == PropertyNotify && event->xproperty.window == root &&
               event->xproperty.atom == owner_property) {
        Atom type;
        int format, ours;
        unsigned long count, left;
        unsigned char *data = NULL;

        XGetWindowProperty(display, root, owner_property, 0, 1, False, XA_WINDOW,
                           &type, &format, &count, &left, &data);
        ours = type == XA_WINDOW && format == 32 && count == 1 && data &&
               *(unsigned long *)data == header;
        if (data) {
            XFree(data);
        }
        if (!ours) {
            helios_desktop_close();
        }
    }
    return 0;
}

int helios_desktop_tick(void)
{
    time_t now;
    struct tm *local;
    char current[80];

    if (!active) {
        return -1;
    }
    now = time(NULL);
    local = localtime(&now);
    if (local && strftime(current, sizeof(current), "%a %d %b %Y  %H:%M %Z", local) &&
        strcmp(stamp, current)) {
        strcpy(stamp, current);
        if (cached) {
            paint_clock(header_pixmap);
            if (width > 440) {
                XClearArea(display, header, width - 320, 16, 296, 30, False);
            }
        } else {
            paint_clock(header);
        }
        XFlush(display);
    }
    return (60 - (now % 60)) * 1000;
}
