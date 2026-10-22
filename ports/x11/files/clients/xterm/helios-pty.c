/* Native Helios terminal backend for MIT X11R4 xterm. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <syslib.h>
#include <nonansi.h>
#include <attrib.h>
#include <task.h>
#include <helios/unix/channel.h>
#include <helios/unix/spawn.h>
#include "helios-pty.h"

/* The display client selects the terminal emulation used by ttyserv. */
#ifndef HELIOS_PTY_TERM
#define HELIOS_PTY_TERM "xterm"
#endif

static int server_pid=-1, child_pid=-1, master_fd=-1, slave_fd=-1;
static char tty_path[256];

void helios_pty_close(void)
{
    if (child_pid>0) { kill(child_pid,SIGTERM); child_pid=-1; }
    if (master_fd>=0) { close(master_fd); master_fd=-1; }
    if (slave_fd>=0) { close(slave_fd); slave_fd=-1; }
    if (server_pid>0) { kill(server_pid,SIGTERM); server_pid=-1; }
}

void helios_pty_resize(int fd, int columns, int rows)
{
    Attributes a;
    if (fd<0 || columns<1 || rows<1) return;
    if (GetAttributes(fdstream(fd),&a)<0) return;
    a.Time=columns; a.Min=rows;
    if (SetAttributes(fdstream(fd),&a)<0)
        fprintf(stderr,"xterm: cannot update terminal dimensions\n");
}

int helios_pty_open(int columns,int rows)
{
    int pipefd[2], pid, i, n, expected, error;
    struct hu_spawn_actions actions;
    struct hu_spawn_attr attr={HU_SPAWN_LOCAL,0,0,3};
    char machine[128], name[32], master[256], discard[256];
    char *args[]={"xterm-ttyserv","-p","-n",name,"-t",HELIOS_PTY_TERM,NULL};
    atexit(helios_pty_close);
    MachineName(machine);
    sprintf(name,"tty.x%x",MyTask->Port);
    sprintf(tty_path,"%s/%s",machine,name);
    sprintf(master,"%s/pty.x%x/entry",machine,MyTask->Port);
    /* Native pipe close can wait for its ten-second synchronous close
     * timeout after this handshake. Channels avoid that delay; explicit
     * spawn also preserves the inherited socket's ownership correctly. */
    if(hu_channel(pipefd)<0) return -1;
    hu_spawn_actions_init(&actions);
    error=hu_spawn_adddup2(&actions,pipefd[1],1);
    if(!error)error=hu_spawn(&pid,"/helios/lib/xterm-ttyserv",
                             &actions,&attr,args,NULL);
    hu_spawn_actions_destroy(&actions);
    close(pipefd[1]);
    if(error) {close(pipefd[0]); errno=error; return -1;}
    server_pid=pid;
    /* Consume both startup name messages, allowing the device to register.
     * The terminal server writes names without separators. */
    n=0;expected=strlen(tty_path)+strlen(master)-6;
    while(n<expected) {
        int count=expected-n;
        if(count>sizeof(discard))count=sizeof(discard);
        i=Read(fdstream(pipefd[0]),discard,count,5*OneSec);
        if(i<=0) break;
        n+=i;
    }
    close(pipefd[0]);
    if(n!=expected) {helios_pty_close(); errno=EIO; return -1;}
    for(i=0;i<50;i++) {
        master_fd=open(master,O_RDWR);
        if(master_fd>=0)break;
        Delay(OneSec/10);
    }
    if(master_fd<0 || master_fd>=32) {
        fprintf(stderr,"xterm: cannot use master terminal %s\n",master);
        helios_pty_close();return -1;
    }
    strcat(tty_path,"/window");
    slave_fd=open(tty_path,O_RDWR|O_CREAT,0666);
    if(slave_fd<0) {fprintf(stderr,"slave open failed: %s\n",tty_path);helios_pty_close();return -1;}
    /* A window must exist before DeviceWindowSetInfo can set its dimensions. */
    helios_pty_resize(master_fd,columns,rows);
    fcntl(master_fd,F_SETFD,FD_CLOEXEC);
    fcntl(slave_fd,F_SETFD,FD_CLOEXEC);
    return master_fd;
}

int helios_pty_spawn(char **command,unsigned long window,char *display,char *termname)
{
    static char termenv[128],windowenv[64],displayenv[256];
    char *fallback[3];
    extern char **environ;
    char **old_environ=environ, **copy;
    int pid,i;
    Environ *env=getenviron();
    Object *old_console=env->Objv[OV_Console], *old_server=env->Objv[OV_CServer];
    Object *console=Locate(NULL,tty_path), *server;
    char parent[256], *slash;
    if(!console)return -1;
    strcpy(parent,tty_path);slash=strrchr(parent,'/');*slash=0;
    server=Locate(NULL,parent);
    if(!server){Close(console);return -1;}
    if ((termname && strlen(termname)>sizeof(termenv)-6) || strlen(display)>sizeof(displayenv)-9) {
        Close(console);Close(server);errno=E2BIG;return -1;
    }
    sprintf(termenv,"TERM=%s",termname?termname:"xterm");
    sprintf(windowenv,"WINDOWID=%u",(unsigned)window);
    sprintf(displayenv,"DISPLAY=%s",display);
    /* BSD putenv appends a new value twice and overruns its allocation.
     * Build our own environment vector; inherited strings remain owned by
     * the runtime. This also avoids reallocating its initial vector. */
    {
        int count=0, used=0;
        while(environ[count])count++;
        copy=malloc((count+4)*sizeof(char *));
        if(!copy){Close(console);Close(server);return -1;}
        for(i=0;i<count;i++)
            if(strncmp(environ[i],"TERM=",5) &&
               strncmp(environ[i],"WINDOWID=",9) &&
               strncmp(environ[i],"DISPLAY=",8)) copy[used++]=environ[i];
        copy[used++]=termenv;copy[used++]=windowenv;copy[used++]=displayenv;
        copy[used]=NULL;environ=copy;
    }
    fallback[0]="/helios/bin/shell";fallback[1]=NULL;
    if(!command)command=fallback;
    /* A program started by -e may never call tcsetpgrp itself. Register
     * this terminal while xterm still has the slave descriptor; the POSIX
     * handler forwards terminal signals to the child process group. A
     * shell or application can subsequently take over the event port. */
    if(tcsetpgrp(slave_fd,getpgrp())<0) {
        fprintf(stderr,"xterm: cannot enable terminal events: %s\n",strerror(errno));
        environ=old_environ;free(copy);
        close(slave_fd);slave_fd=-1;
        Close(console);Close(server);return -1;
    }
    env->Objv[OV_Console]=console;env->Objv[OV_CServer]=server;
    pid=vfork();
    if(pid==0) {
        dup2(slave_fd,0);dup2(slave_fd,1);dup2(slave_fd,2);
        for(i=3;i<64;i++)close(i);
        signal(SIGINT,SIG_DFL);signal(SIGQUIT,SIG_DFL);signal(SIGTERM,SIG_DFL);
        signal(SIGHUP,SIG_DFL);signal(SIGCHLD,SIG_DFL);
        execvp(command[0],command);
        fprintf(stderr,"xterm: cannot execute %s: %s\n",command[0],strerror(errno));
        _exit(127);
    }
    environ=old_environ;free(copy);
    if(pid<0){close(slave_fd);slave_fd=-1;}
    env->Objv[OV_Console]=old_console;env->Objv[OV_CServer]=old_server;
    Close(console);Close(server);
    /* Keep the registration stream open: ttyserv removes its event port
     * when the owning stream closes. Children receive only fd 0/1/2. */
    child_pid=pid;
    return pid;
}

int helios_pty_read(int fd,char *buffer,int size)
{
    unsigned char packet[4096];
    int n,i,used=0,limit=size/2+1;
    if(limit>sizeof(packet))limit=sizeof(packet);
    if(size<2){errno=EINVAL;return -1;}
    n=read(fd,packet,limit);
    /* tpseudo.d prefixes each read with a packet-control byte. */
    if(n<=0)return n;
    if(packet[0]!=0 || n==1){errno=EAGAIN;return -1;}
    for(i=1;i<n;i++) {
        /* Native Helios applications use eight-bit CSI and SS3. */
        if(packet[i]==0x9b || packet[i]==0x8f) {
            buffer[used++]=0x1b;
            buffer[used++]=packet[i]==0x9b?'[':'O';
        } else buffer[used++]=packet[i];
    }
    return used;
}
