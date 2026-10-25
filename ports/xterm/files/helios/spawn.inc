/* SPDX-License-Identifier: MIT */
/* Native Helios task startup; main.c retains upstream Xt setup. */
static int
spawnXTerm(XtermWidget xw, unsigned line_speed GCC_UNUSED)
{
    TScreen *screen = TScreenOf(xw);
    if (am_slave >= 0) { xtermWarning("-S is unavailable on Helios\n"); return 1; }
    XtRealizeWidget(XtParent(xw));
    screen->uid=getuid(); screen->gid=getgid();
    screen->respond=helios_pty_open(MaxCols(screen),MaxRows(screen));
    if(screen->respond<0) {perror("xterm: terminal creation");exit(1);}
    screen->pid=helios_pty_spawn(command_to_exec,XtWindow(XtParent(xw)),
        DisplayString(screen->display),resource.term_name);
    if(screen->pid<0) {perror("xterm: execute");helios_pty_close();exit(1);}
    signal(SIGINT,SIG_IGN);signal(SIGQUIT,SIG_IGN);
    signal(SIGTERM,Exit);signal(SIGHUP,Exit);
    return 0;
}
