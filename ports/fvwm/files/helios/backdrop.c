/* Server-rendered backdrop with a small resident clock. Only its header
 * rectangle is repainted when the displayed minute changes. */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslib.h>
#include "helios-logo-mask.h"
static unsigned long color(Display *d, const char *name)
{
    XColor c; Colormap map=DefaultColormap(d,DefaultScreen(d));
    if (!XParseColor(d,map,name,&c) || !XAllocColor(d,map,&c))
        return BlackPixel(d,DefaultScreen(d));
    return c.pixel;
}
/* Antialiased vector logo using only core X11, including vintage displays.
 * Group adjacent pixels by coverage and send one rectangle batch per shade. */
static void logo(Display *d, Drawable p, GC gc)
{
    XRectangle spans[logo_width*logo_height];
    XColor c;
    int level,x,y,start,n;
    for (level=1;level<=15;level++) {
        c.red=(41+(237-41)*level/15)*257;
        c.green=(67+(232-67)*level/15)*257;
        c.blue=(76+(216-76)*level/15)*257;
        if (!XAllocColor(d,DefaultColormap(d,DefaultScreen(d)),&c)) continue;
        XSetForeground(d,gc,c.pixel);n=0;
        for (y=0;y<logo_height;y++) for (x=0;x<logo_width;) {
            if (logo_alpha[y*logo_width+x]!=level) {x++;continue;}
            start=x++;
            while (x<logo_width && logo_alpha[y*logo_width+x]==level) x++;
            spans[n].x=22+start;spans[n].y=11+y;
            spans[n].width=x-start;spans[n].height=1;n++;
        }
        if (n) XFillRectangles(d,p,gc,spans,n);
    }
}
int main(void)
{
    Display *d=XOpenDisplay(0);
    Window root; Pixmap p,token;
    GC gc; XFontStruct *font;
    int s,w,h,i; unsigned long n,after; int format;
    unsigned long header,ink;
    char previous[80]="",stamp[80];
    Atom property,type; unsigned char *old=0;
    if (!d) return 1;
    s=DefaultScreen(d); root=RootWindow(d,s);
    w=DisplayWidth(d,s); h=DisplayHeight(d,s);
    property=XInternAtom(d,"_XSETROOT_ID",False);
    if (XGetWindowProperty(d,root,property,0,1,True,AnyPropertyType,
                          &type,&format,&n,&after,&old)==Success) {
        if (type==XA_PIXMAP && format==32 && n==1)
            XKillClient(d,*(unsigned long *)old);
        if (old) XFree(old);
    }
    p=XCreatePixmap(d,root,w,h,DefaultDepth(d,s));
    gc=XCreateGC(d,p,0,0);
    XSetForeground(d,gc,color(d,"#304e57"));
    XFillRectangle(d,p,gc,0,0,w,h);
    header=color(d,"#29434c");ink=color(d,"#a9c0bf");
    XSetForeground(d,gc,header);
    XFillRectangle(d,p,gc,0,0,w,64);
    XSetForeground(d,gc,color(d,"#ede8d8"));
    logo(d,p,gc);
    font=XLoadQueryFont(d,"fixed");
    if (font) XSetFont(d,gc,font->fid);
    /* Quiet geometry in the free space, without competing with windows. */
    XSetForeground(d,gc,color(d,"#3b5c63"));
    for (i=0;i<5;i++) XDrawArc(d,p,gc,w-390-i*32,h-420-i*32,
                              300+i*64,300+i*64,0,360*64);
    XSetWindowBackgroundPixmap(d,root,p);XClearWindow(d,root);
    token=XCreatePixmap(d,root,1,1,1);
    XChangeProperty(d,root,property,XA_PIXMAP,32,PropModeReplace,
                    (unsigned char *)&token,1);
    XSetCloseDownMode(d,RetainPermanent);
    for (;;) {
        time_t now=time(NULL);
        struct tm *tm=localtime(&now);
        if (tm && strftime(stamp,sizeof(stamp),"%a %d %b %Y  %H:%M %Z",tm)
            && strcmp(stamp,previous)) {
            if (w>440) {
                int width=font?XTextWidth(font,stamp,strlen(stamp)):strlen(stamp)*6;
                XSetForeground(d,gc,header);
                XFillRectangle(d,p,gc,w-320,16,296,30);
                XSetForeground(d,gc,ink);
                XDrawString(d,p,gc,w-28-width,36,stamp,strlen(stamp));
                /* Reinstall the background after modifying its pixmap:
                 * servers may cache the previously assigned contents. */
                XSetWindowBackgroundPixmap(d,root,None);
                XSetWindowBackgroundPixmap(d,root,p);
                XClearArea(d,root,w-320,16,296,30,False);
                XCopyArea(d,p,root,gc,w-320,16,296,30,w-320,16);
            }
            strcpy(previous,stamp);
            XFlush(d);
        }
        /* One round trip a second notices a closed display without busy
         * polling the remote socket. This client requests no input events. */
        XSync(d,True);
        Delay(1000000);
    }
    if (font) XFreeFont(d,font);
    XFreeGC(d,gc);
    XCloseDisplay(d);
    return 0;
}
