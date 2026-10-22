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
#include "dm.h"

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
