/* The root uses a solid background. Paint its arcs on exposure, and draw
 * logo/clock in a small override-redirect window: no screen-sized pixmap. */
#include "desktop.h"
#include <X11/Xatom.h>
#include <string.h>
#include <time.h>
#include "helios-logo-mask.h"

static Display *display;
static Window root,header;
static Pixmap token;
static Atom owner_property;
static GC gc;
static XFontStruct *font;
static int own_font,width,height,active;
static unsigned long colors[24];
static int color_count;
static unsigned long background,header_color,ink,pattern,shades[16];
static char stamp[80];

static unsigned long remember_color(XColor *c)
{
    if (XAllocColor(display,DefaultColormap(display,DefaultScreen(display)),c)) {
        colors[color_count++]=c->pixel;
        return c->pixel;
    }
    return BlackPixel(display,DefaultScreen(display));
}
static unsigned long named_color(const char *name)
{
    XColor c;
    if (!XParseColor(display,DefaultColormap(display,DefaultScreen(display)),name,&c))
        return BlackPixel(display,DefaultScreen(display));
    return remember_color(&c);
}
static void paint_logo(void)
{
    XRectangle spans[64];
    int level,x,y,start,n;
    for (level=1;level<=15;level++) {
        XSetForeground(display,gc,shades[level]);n=0;
        for (y=0;y<logo_height;y++) for (x=0;x<logo_width;) {
            if (logo_alpha[y*logo_width+x]!=level) {x++;continue;}
            start=x++;
            while (x<logo_width && logo_alpha[y*logo_width+x]==level) x++;
            spans[n].x=22+start;spans[n].y=11+y;
            spans[n].width=x-start;spans[n].height=1;
            if (++n==64) {
                XFillRectangles(display,header,gc,spans,n);n=0;
            }
        }
        if (n) XFillRectangles(display,header,gc,spans,n);
    }
}
static void paint_pattern(void)
{
    int i;
    XSetForeground(display,gc,pattern);
    for (i=0;i<5;i++) XDrawArc(display,root,gc,width-390-i*32,height-420-i*32,
                              300+i*64,300+i*64,0,360*64);
}
static void paint_clock(void)
{
    if (width>440) {
        int pixels=font?XTextWidth(font,stamp,strlen(stamp)):strlen(stamp)*6;
        XSetForeground(display,gc,header_color);
        XFillRectangle(display,header,gc,width-320,16,296,30);
        XSetForeground(display,gc,ink);
        XDrawString(display,header,gc,width-28-pixels,36,stamp,strlen(stamp));
    }
}
static void paint_header(void)
{
    XClearWindow(display,header);
    paint_logo();paint_clock();XFlush(display);
}
void helios_desktop_close(void)
{
    if (!display) return;
    active=0;
    if (header) XDestroyWindow(display,header);
    if (token) XFreePixmap(display,token);
    if (gc) XFreeGC(display,gc);
    if (font && own_font) XFreeFont(display,font);
    if (color_count) XFreeColors(display,DefaultColormap(display,DefaultScreen(display)),
                                colors,color_count,0);
    display=0;header=0;token=0;gc=0;font=0;color_count=0;
}
int helios_desktop_open(Display *d, XFontStruct *existing_font)
{
    XSetWindowAttributes attributes;
    XWindowAttributes root_attributes;
    XColor c;
    Atom property;
    int level;
    helios_desktop_close();display=d;root=DefaultRootWindow(d);
    width=DisplayWidth(d,DefaultScreen(d));height=DisplayHeight(d,DefaultScreen(d));
    background=named_color("#304e57");header_color=named_color("#29434c");
    ink=named_color("#a9c0bf");pattern=named_color("#3b5c63");
    for (level=1;level<=15;level++) {
        c.red=(41+(237-41)*level/15)*257;
        c.green=(67+(232-67)*level/15)*257;
        c.blue=(76+(216-76)*level/15)*257;
        shades[level]=remember_color(&c);
    }
    attributes.override_redirect=True;
    attributes.background_pixel=header_color;
    attributes.event_mask=ExposureMask;
    header=XCreateWindow(d,root,0,0,width,64,0,CopyFromParent,InputOutput,
                         CopyFromParent,CWOverrideRedirect|CWBackPixel|CWEventMask,
                         &attributes);
    XStoreName(d,header,"Helios-Backdrop");
    gc=XCreateGC(d,root,0,0);
    own_font=existing_font==0;
    font=existing_font?existing_font:XLoadQueryFont(d,"fixed");
    if (font) XSetFont(d,gc,font->fid);
    XGetWindowAttributes(d,root,&root_attributes);
    XSelectInput(d,root,root_attributes.your_event_mask|ExposureMask|PropertyChangeMask);
    XSetWindowBackground(d,root,background);XClearWindow(d,root);
    XMapWindow(d,header);XLowerWindow(d,header);
    owner_property=XInternAtom(d,"_HELIOS_BACKDROP_OWNER",False);
    XChangeProperty(d,root,owner_property,XA_WINDOW,32,PropModeReplace,
                    (unsigned char *)&header,1);
    /* Let older standalone backdrops see the replacement and close normally. */
    token=XCreatePixmap(d,root,1,1,1);
    property=XInternAtom(d,"_XSETROOT_ID",False);
    XChangeProperty(d,root,property,XA_PIXMAP,32,PropModeReplace,
                    (unsigned char *)&token,1);
    active=1;stamp[0]=0;helios_desktop_tick();
    paint_pattern();paint_header();return 1;
}
int helios_desktop_event(XEvent *event)
{
    if (!active) return 0;
    if (event->type==Expose) {
        if (event->xexpose.window==header) {
            if (!event->xexpose.count) paint_header();
            return 1;
        }
        if (event->xexpose.window==root && !event->xexpose.count) {
            paint_pattern();XFlush(display);
        }
    } else if (event->type==PropertyNotify && event->xproperty.window==root &&
               event->xproperty.atom==owner_property) {
        Atom type;
        int format,ours;
        unsigned long count,left;
        unsigned char *data=0;
        XGetWindowProperty(display,root,owner_property,0,1,False,XA_WINDOW,
                           &type,&format,&count,&left,&data);
        ours=type==XA_WINDOW && format==32 && count==1 &&
             *(unsigned long *)data==header;
        if (data) XFree(data);
        if (!ours) helios_desktop_close();
    }
    return 0;
}
int helios_desktop_tick(void)
{
    time_t now;
    struct tm *tm;
    char current[80];
    if (!active) return -1;
    now=time(NULL);tm=localtime(&now);
    if (tm && strftime(current,sizeof(current),"%a %d %b %Y  %H:%M %Z",tm) &&
        strcmp(stamp,current)) {
        strcpy(stamp,current);paint_clock();XFlush(display);
    }
    return (60-(now%60))*1000;
}
