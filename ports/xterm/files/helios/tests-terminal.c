/* SPDX-License-Identifier: MIT */
/* Run inside xterm; write observations independently of the display. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <syslib.h>
#include <nonansi.h>
#include <helios/unix/api.h>
static volatile sig_atomic_t resized;
static void winch(int s) { (void)s; resized=1; }
static int report(FILE *f,const char *name,const char *text)
{
    unsigned char reply[64]; int n=0,r;
    printf("\033[2J\033[H%s\033[6n",text);fflush(stdout);
    while(n<sizeof(reply)-1) {
        r=Read(fdstream(0),reply+n,1,3*OneSec);
        if(r!=1)break;
        if(reply[n++]=='R')break;
    }
    reply[n]=0;fprintf(f,"%s:",name);
    for(r=0;r<n;r++)fprintf(f," %02x",reply[r]);
    fputc('\n',f);fflush(f);
    return n;
}
int main(int argc,char **argv)
{
    struct termios saved;FILE *f;unsigned rows,cols;unsigned char key[32];
    int n,used=0,paste=argc==3 && !strcmp(argv[2],"--paste");
    if((argc!=2 && !paste) || !(f=fopen(argv[1],"w")))return 2;
    if(hu_terminal_raw(0,&saved)<0)return 3;
    signal(SIGWINCH,winch);
    hu_terminal_size(0,&rows,&cols);fprintf(f,"initial=%ux%u\n",cols,rows);fflush(f);
    if(!paste) {
    report(f,"osc-st","\033]0;Xterm412-check\033\\");
    report(f,"unknown-osc","\033]133;A\033\\");
    report(f,"osc-bel","\033]0;Xterm412-check\007");
    report(f,"latin1-default","\351\344");
    report(f,"utf8","\033%G\303\251\316\251\344\270\255");
    report(f,"utf8-c1-bytes","\303\233\304\217");
    report(f,"native-csi-utf8","\233" "2C");
    report(f,"latin1-restored","\033%@\351\344");
    printf("\033%%G");
    printf("\033[2J\033[H\033[38;5;196mRED\033[0m UTF-8: \303\251 \316\251 \344\270\255\r\nKEY AND RESIZE TEST READY\r\n");fflush(stdout);
    } else {
        printf("\033[?2004hPASTE TEST READY\r\n");fflush(stdout);
        report(f,"paste-barrier","");
        fprintf(f,"paste-ready\n");fflush(f);
    }
    while(used<sizeof(key)) {
        fd_set r;struct timeval timeout;
        FD_ZERO(&r);FD_SET(0,&r);timeout.tv_sec=15;timeout.tv_usec=0;
        n=select(1,&r,NULL,NULL,&timeout);
        if(resized) {
            resized=0;hu_terminal_size(0,&rows,&cols);
            fprintf(f,"resize=%ux%u\n",cols,rows);fflush(f);
        }
        if(n<0 && errno==EINTR)continue;
        if(n<=0)break;
        n=read(0,key+used,1);
        if(n<0 && errno==EINTR)continue;
        if(n!=1)break;
        if(key[used++]==4)break;
    }
    fprintf(f,"keys:");for(n=0;n<used;n++)fprintf(f," %02x",key[n]);fputc('\n',f);fclose(f);
    if(paste){printf("\033[?2004l");fflush(stdout);}
    hu_terminal_restore(0,&saved);
    return 0;
}
