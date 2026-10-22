#include <sys/wait.h>
#include "helios-pty.h"

first_map_occurred()
{
    waiting_for_initial_map=False;
    helios_pty_resize(term->screen.respond,term->screen.max_col+1,term->screen.max_row+1);
}

spawn()
{
    TScreen *screen=&term->screen;
    screen->respond=-1;
    if(ConnectionNumber(screen->display)>=32) {
        fprintf(stderr,"xterm: X connection exceeds the X11R4 descriptor-mask limit\n");exit(1);
    }
    screen->uid=getuid();screen->gid=getgid();
    if(am_slave) {fprintf(stderr,"xterm: -S is not supported by the Helios backend\n");exit(1);}
    XtRealizeWidget(screen->TekEmu?XtParent(tekWidget):XtParent(term));
    screen->respond=helios_pty_open(screen->max_col+1,screen->max_row+1);
    if(screen->respond<0) {perror("xterm: terminal creation");exit(1);}
    waiting_for_initial_map=False;
    screen->pid=helios_pty_spawn(command_to_exec,
        XtWindow(screen->TekEmu?XtParent(tekWidget):XtParent(term)),
        DisplayString(screen->display),resource.term_name);
    if(screen->pid<0) {perror("xterm: execute");helios_pty_close();exit(1);}
    signal(SIGINT,SIG_IGN);signal(SIGQUIT,SIG_IGN);
    signal(SIGTERM,Exit);signal(SIGHUP,Exit);
    return 0;
}

SIGNAL_T Exit(int n)
{
    helios_pty_close();
    if(term->screen.logging)CloseLog(&term->screen);
    exit(n);
}

static SIGNAL_T reapchild()
{
    int status,pid;
    while((pid=waitpid(-1,&status,WNOHANG))>0)
        if(pid==term->screen.pid)Exit(0);
}
