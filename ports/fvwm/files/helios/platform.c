#include "platform.h"
#include <stdio.h>
#include <syslib.h>
#include <gsp.h>
#include "command.h"
extern Stream *fdstream(int);

static int (*idle_hook)(void);
void ktm_set_idle_hook(int (*hook)(void)) { idle_hook=hook; }

int ktm_select(int width,fd_set *in,fd_set *out,fd_set *except,struct timeval *tv)
{
    struct hu_pollfd fds[FD_SETSIZE];
    struct timeval idle_timeout;
    int i,n=0,result,found=0,timeout=-1;
    if(width<0 || width>FD_SETSIZE){errno=EINVAL;return -1;}
    if (idle_hook) {
        int ms=idle_hook();
        if (ms>=0 && (!tv || tv->tv_sec>ms/1000 ||
            (tv->tv_sec==ms/1000 && tv->tv_usec>(ms%1000)*1000))) {
            idle_timeout.tv_sec=ms/1000;idle_timeout.tv_usec=(ms%1000)*1000;
            tv=&idle_timeout;
        }
    }
    /* Native X connections become Helios pipes after Connect. They have
     * ordinary SelectStream readiness, without the extended socket/FIFO
     * status used by hu_poll. Select the whole set together in that case. */
    for(i=0;i<width;i++) {
        Stream *s;
        if(!((in&&FD_ISSET(i,in)) || (out&&FD_ISSET(i,out)) ||
             (except&&FD_ISSET(i,except))))continue;
        s=fdstream(i);
        if(s && (s->Type & ~Type_Flags)==(Type_Pipe & ~Type_Flags))
            return select(width,in,out,except,tv);
    }
    if(tv)timeout=tv->tv_sec*1000+(tv->tv_usec+999)/1000;
    for(i=0;i<width;i++) {
        int events=0;
        if(in&&FD_ISSET(i,in))events|=HU_POLLIN;
        if(out&&FD_ISSET(i,out))events|=HU_POLLOUT;
        if(except&&FD_ISSET(i,except))events|=HU_POLLPRI;
        if(events){fds[n].fd=i;fds[n].events=events;n++;}
    }
    result=hu_poll(fds,n,timeout);
    if(result<0)return -1;
    if(in)FD_ZERO(in);if(out)FD_ZERO(out);if(except)FD_ZERO(except);
    for(i=0;i<n;i++) {
        int re=fds[i].revents,ev=fds[i].events;
        if(re&HU_POLLNVAL){errno=EBADF;return -1;}
        if((ev&HU_POLLIN)&&(re&(HU_POLLIN|HU_POLLHUP|HU_POLLERR))){FD_SET(fds[i].fd,in);found++;}
        if((ev&HU_POLLOUT)&&(re&(HU_POLLOUT|HU_POLLHUP|HU_POLLERR))){FD_SET(fds[i].fd,out);found++;}
        if((ev&HU_POLLPRI)&&(re&HU_POLLPRI)){FD_SET(fds[i].fd,except);found++;}
    }
    return found;
}

/* KTM's core map reserves Mod2 for Num Lock. Modern X servers can retain
 * that locked state after the key's modifier mapping has been cleared. */
int ktm_grab_key(Display *d,int key,unsigned mods,Window w,Bool owner,int pointer,int keyboard)
{
    XGrabKey(d,key,mods,w,owner,pointer,keyboard);
    if (mods!=AnyModifier && !(mods&Mod2Mask)) XGrabKey(d,key,mods|Mod2Mask,w,owner,pointer,keyboard);
    return 0;
}
int ktm_grab_button(Display *d,unsigned button,unsigned mods,Window w,Bool owner,
                     unsigned events,int pointer,int keyboard,Window confine,Cursor cursor)
{
    XGrabButton(d,button,mods,w,owner,events,pointer,keyboard,confine,cursor);
    if (mods!=AnyModifier && !(mods&Mod2Mask))
        XGrabButton(d,button,mods|Mod2Mask,w,owner,events,pointer,keyboard,confine,cursor);
    return 0;
}
int ktm_ungrab_button(Display *d,unsigned button,unsigned mods,Window w)
{
    XUngrabButton(d,button,mods,w);
    if (mods!=AnyModifier && !(mods&Mod2Mask)) XUngrabButton(d,button,mods|Mod2Mask,w);
    return 0;
}

/* The vintage BSD putenv overruns its binding buffer when appending HOSTDISPLAY.
 * Retain putenv's string ownership while owning only our new vector. */
int ktm_putenv(char *entry)
{
    extern char **environ;
    static char **owned;
    char **copy,*eq=strchr(entry,'=');
    unsigned count=0,used=0,i,length;
    if (!eq) { errno=EINVAL; return -1; }
    length=eq-entry+1;
    while (environ[count]) count++;
    copy=malloc((count+2)*sizeof(*copy));
    if (!copy) { errno=ENOMEM; return -1; }
    for (i=0;i<count;i++)
        if (strncmp(environ[i],entry,length)) copy[used++]=environ[i];
    copy[used++]=entry;copy[used]=0;
    free(owned);owned=copy;environ=copy;
    return 0;
}

/* Channels are byte streams: packet boundaries are not read boundaries.
 * Call after select reports input; bound a stalled partial packet to 5 s. */
int ktm_read_full(int fd, void *buffer, unsigned size)
{
    unsigned done = 0;
    while (done < size) {
        struct hu_pollfd p;
        int n;
        p.fd=fd; p.events=HU_POLLIN; p.revents=0;
        n=hu_poll(&p,1,5000);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        n=read(fd,(char *)buffer+done,size-done);
        if (n < 0 && (errno==EINTR || errno==EAGAIN || errno==EWOULDBLOCK)) continue;
        if (n <= 0) return -1;
        done+=n;
    }
    return done;
}
int ktm_write_full(int fd, const void *buffer, unsigned size)
{
    unsigned done=0;
    while (done < size) {
        int n=write(fd,(const char *)buffer+done,size-done);
        if (n < 0 && errno==EINTR) continue;
        if (n <= 0) return -1;
        done+=n;
    }
    return done;
}
void ktm_exec(const char *command)
{
    struct hu_spawn_attr attr={HU_SPAWN_DEFAULT,0,0,3};
    char *args[64],*copy=malloc(strlen(command)+1);
    pid_t pid;
    int count,error;
    if (!copy) { errno=ENOMEM;perror("fvwm: launch");return; }
    strcpy(copy,command);
    count=helios_command_split(copy,args,64);
    if (count==0) { free(copy);return; }
    /* Absolute/relative executable paths avoid shell builtins and aliases.
     * Keep shell interpretation for expansions, pipelines and scripts. */
    if (count>0 && strchr(args[0],'/')) {
        error=hu_spawnp(&pid,args[0],0,&attr,args,0);
        if (!error) { free(copy);return; }
        if (error!=ENOEXEC) {
            free(copy);errno=error;perror("fvwm: launch");return;
        }
    }
    free(copy);
    args[0]="/helios/bin/shell";args[1]="-f";args[2]="-c";
    args[3]=(char *)command;args[4]=0;
    error=hu_spawn(&pid,args[0],0,&attr,args,0);
    if (error) { errno=error;perror("fvwm: launch"); }
}
void ktm_restart(const char *command, char **args)
{
    struct hu_spawn_attr attr={HU_SPAWN_LOCAL,0,0,3};
    pid_t pid;
    int status, error=hu_spawnp(&pid,command,0,&attr,args,0);
    if (error) { errno=error; return; }
    /* A login manager may be waiting for this exact desktop task. Console
     * desktops already have a persistent shell/TFM and can release the old
     * manager's memory instead of keeping a waiter on every restart. */
    if (!getenv("FVWM_RESTART_WAIT") || strcmp(getenv("FVWM_RESTART_WAIT"),"0"))
        while (waitpid(pid,&status,0)<0 && errno==EINTR) {}
    exit(0);
}
