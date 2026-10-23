/* Separate exec image: Helios vfork cannot run a long-lived session in the
 * parent's address space. The master persists and handles XDMCP keepalives. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <X11/Xlib.h>
#include <X11/Xresource.h>
#include "dm.h"
#include "helios-logo-mask.h"

int KtmKillGroup(int group, int sig) { return kill(-group, sig); }

/* Apply the same numeric core map as Xsession before creating the login
 * widget. A post-login xmodmap cannot repair the greeter's keyboard. */
void KtmLoginKeymap(Display *display)
{
    FILE *f = fopen("/helios/etc/xdm/german-core.xmodmap", "r");
    char line[512], *p, *end;
    KeySym symbols[16];
    XModifierKeymap *modifiers;
    XMappingEvent changed;
    int code, i, low, high;
    if (!f) { LogError("Cannot open login keyboard map\n"); return; }
    XDisplayKeycodes(display, &low, &high);
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "keycode %d", &code) != 1 ||
            code < low || code > high || !(p = strchr(line, '='))) continue;
        ++p;
        for (i = 0; i < 16; ++i) {
            symbols[i] = strtoul(p, &end, 0);
            if (end == p) break;
            p = end;
        }
        if (i == 16) XChangeKeyboardMapping(display, code, 16, symbols, 1);
    }
    fclose(f);
    modifiers = XGetModifierMapping(display);
    if (modifiers) {
        /* The compatibility map ends with "clear mod2" (Num Lock). */
        for (i = 0; i < modifiers->max_keypermod; ++i)
            modifiers->modifiermap[4 * modifiers->max_keypermod + i] = 0;
        if (XSetModifierMapping(display, modifiers) != MappingSuccess)
            LogError("Login keyboard modifier map is busy\n");
        XFreeModifiermap(modifiers);
    }
    XSync(display, False);
    /* XtOpenDisplay may already have caused Xlib to cache the original map.
     * Invalidate it explicitly before the login widget translates any keys. */
    memset(&changed, 0, sizeof(changed));
    changed.display=display; changed.type=MappingNotify;
    changed.request=MappingKeyboard; changed.first_keycode=low;
    changed.count=high-low+1;
    XRefreshKeyboardMapping(&changed);
    changed.request=MappingModifier;
    XRefreshKeyboardMapping(&changed);
}

static unsigned long KtmColor(Display *d, char *name)
{
    XColor c;
    Colormap map=DefaultColormap(d,DefaultScreen(d));
    if (!XParseColor(d,map,name,&c) || !XAllocColor(d,map,&c))
        return BlackPixel(d,DefaultScreen(d));
    return c.pixel;
}

/* The Helios logo as in helios-backdrop: spans grouped by coverage level,
 * shaded from the header colour to the logo cream. */
static void KtmLogo(Display *d, Drawable p, GC gc, int ox, int oy)
{
    static XRectangle spans[logo_width*logo_height];
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
            spans[n].x=ox+start;spans[n].y=oy+y;
            spans[n].width=x-start;spans[n].height=1;n++;
        }
        if (n) XFillRectangles(d,p,gc,spans,n);
    }
}

/* Give the greeter the look of the Helios desktop: paint the root window
 * like helios-backdrop, and add /helios/etc/xdm/Xresources to the display's
 * resource database before the login widget is created. The root
 * background outlives this connection; the session replaces it. */
void KtmLoginStyle(Display *d)
{
    static char banner[]="Helios 1.31-SP2  -  KTM transputer workstation";
    XrmDatabase resources;
    XFontStruct *font;
    Window root;
    Pixmap p;
    GC gc;
    int s,w,h,i;

    resources=XrmGetFileDatabase("/helios/etc/xdm/Xresources");
    if (resources) XrmMergeDatabases(resources,&d->db);

    s=DefaultScreen(d); root=RootWindow(d,s);
    w=DisplayWidth(d,s); h=DisplayHeight(d,s);
    p=XCreatePixmap(d,root,w,h,DefaultDepth(d,s));
    if (!p) return;
    gc=XCreateGC(d,p,0,0);
    XSetForeground(d,gc,KtmColor(d,"#304e57"));
    XFillRectangle(d,p,gc,0,0,w,h);
    XSetForeground(d,gc,KtmColor(d,"#29434c"));
    XFillRectangle(d,p,gc,0,0,w,64);
    KtmLogo(d,p,gc,22,11);
    XSetForeground(d,gc,KtmColor(d,"#3b5c63"));
    for (i=0;i<5;i++) XDrawArc(d,p,gc,w-390-i*32,h-420-i*32,
                               300+i*64,300+i*64,0,360*64);
    font=XLoadQueryFont(d,"-adobe-helvetica-medium-r-normal--14-*-*-*-p-*-iso8859-1");
    if (!font) font=XLoadQueryFont(d,"fixed");
    if (font && w>480) {
        XSetFont(d,gc,font->fid);
        XSetForeground(d,gc,KtmColor(d,"#a9c0bf"));
        XDrawString(d,p,gc,w-28-XTextWidth(font,banner,strlen(banner)),
                    32+font->ascent/2,banner,strlen(banner));
    }
    XSetWindowBackgroundPixmap(d,root,p);
    XClearWindow(d,root);
    XFreePixmap(d,p);
    if (font) XFreeFont(d,font);
    XFreeGC(d,gc);
    XFlush(d);
}

static char userAuthPath[128];
void KtmRemoveUserAuthorization(void)
{
    if (*userAuthPath) { unlink(userAuthPath); *userAuthPath=0; }
}

void SetUserAuthorization(struct display *d, struct verify_info *verify)
{
    int fd=-1, attempt;
    FILE *f;
    Xauth auth;
    extern char **setEnv(char **, char *, char *);
    if (!d->authorization) exit(1);
    for (attempt=0; attempt<100 && fd<0; ++attempt) {
        sprintf(userAuthPath,"/helios/tmp/.xdm-auth-%d-%d",getpid(),attempt);
        fd=open(userAuthPath,O_WRONLY|O_CREAT|O_EXCL,0600);
    }
    if (fd<0 || !(f=fdopen(fd,"wb"))) exit(1);
    auth=*d->authorization;
    auth.family=FamilyWild; auth.address_length=0; auth.number_length=0;
    if (!XauWriteAuth(f,&auth) || fflush(f)) { fclose(f); unlink(userAuthPath); exit(1); }
    fclose(f);
    verify->userEnviron=setEnv(verify->userEnviron,"XAUTHORITY",userAuthPath);
    verify->systemEnviron=setEnv(verify->systemEnviron,"XAUTHORITY",userAuthPath);
}

int KtmAllowed(struct sockaddr *peer)
{
    FILE *f;
    char line[128];
    unsigned long address;
    if (!peer || peer->sa_family != AF_INET) return 0;
    address=((struct sockaddr_in *)peer)->sin_addr.s_addr;
    f=fopen("/helios/etc/xdm/Xaccess","r");
    if (!f) return 0;
    while (fgets(line,sizeof(line),f)) {
        char *end=strpbrk(line," \t\r\n#");
        if (end) *end=0;
        if (*line && inet_addr(line)==address) { fclose(f); return 1; }
    }
    fclose(f); return 0;
}

int KtmSessionWorker(int argc, char **argv)
{
    struct display *d;
    FILE *f;
    char *resources[4];
    if (argc != 6) return 1;
    resources[0]="xdm"; resources[1]="-config"; resources[2]=argv[5]; resources[3]=0;
    InitResources(3,resources); LoadDMResources();
    d=NewDisplay(argv[2], *argv[3] ? argv[3] : (char *)0);
    if (!d) return 1;
    d->displayType.location=Foreign;
    d->displayType.origin=FromXDMCP;
    d->displayType.lifetime=Transient;
    LoadDisplayResources(d);
    if (!*argv[4] || !(f=fopen(argv[4],"rb"))) return 1;
    d->authorization=XauReadAuth(f); fclose(f);
    if (!d->authorization) return 1;
    signal(SIGPIPE,SIG_IGN);
    SetAuthorization(d);
    if (!WaitForServer(d)) return OPENFAILED_DISPLAY;
    ManageSession(d);
    return REMANAGE_DISPLAY;
}
