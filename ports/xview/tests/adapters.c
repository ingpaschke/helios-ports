/* SPDX-License-Identifier: MIT */
/* Run on Helios: adapters /helios/tmp/an-existing-test-directory */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <xview/notify.h>

extern char *xvh_getcwd(char *, int);
extern int xvh_same_file(int, const char *);
extern int xvh_start_filter(char **, int *, int *);
extern int xvh_setitimer(int, const struct itimerval *, struct itimerval *);
extern int notify_select(int, fd_set *, fd_set *, fd_set *, const struct timeval *);
extern int xvh_fcntl(int, int, ...);
static volatile int alarms;
static void alarmed(int sig) { (void)sig; ++alarms; }
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FAIL line %d errno %d\n",__LINE__,errno); return 1; } } while (0)

static int fd_count(void)
{
    int i, n = 0;
    for (i = 0; i < FD_SETSIZE; ++i) if (fcntl(i,F_GETFD) >= 0) ++n;
    return n;
}

int main(int argc, char **argv)
{
    char *cwd;
    char a[512], b[512], alias[512], missing[512], buffer[32];
    int source, destination, count, pid, input, output, status, before, i;
    struct itimerval timer;
    struct timeval timeout = {2, 0};
    fd_set ready;
    char *cat[] = {"/helios/bin/cat", NULL};
    char *bad[] = {"/helios/bin/no-such-xview-filter", NULL};
    cwd = xvh_getcwd(NULL, 1024); CHECK(cwd && *cwd); free(cwd);
    cwd = xvh_getcwd(NULL, 0); CHECK(cwd && *cwd); free(cwd);
    CHECK(xvh_getcwd(NULL, 1)==NULL && errno==ERANGE);
    puts("PASS allocating current-directory lookup");
    CHECK(argc == 2 && strlen(argv[1]) < 460);
    sprintf(a,"%s/identity.a",argv[1]); sprintf(b,"%s/identity.b",argv[1]);
    sprintf(alias,"%s/./identity.a",argv[1]);
    sprintf(missing,"%s/identity.missing",argv[1]); unlink(missing);
    source = open(a,O_RDWR|O_CREAT|O_TRUNC,0600); CHECK(source >= 0);
    destination = open(b,O_RDWR|O_CREAT|O_TRUNC,0600); CHECK(destination >= 0);
    CHECK(write(source,"original",8)==8 && write(destination,"backup",6)==6);
    close(destination);
    CHECK(xvh_same_file(source,a)==1 && xvh_same_file(source,alias)==1);
    CHECK(xvh_same_file(source,b)==0 && xvh_same_file(source,missing)==0);
    CHECK(xvh_fcntl(-1,F_GETFL)==-1 && errno==EBADF);
    destination = fcntl(source,F_DUPFD,FD_SETSIZE); CHECK(destination >= FD_SETSIZE);
    CHECK(xvh_fcntl(destination,F_SETFL,O_NONBLOCK)==0);
    CHECK((xvh_fcntl(destination,F_GETFL)&O_NONBLOCK)!=0);
    CHECK(xvh_fcntl(destination,F_GETFD)>=0);
    close(destination);
    puts("PASS notifier descriptor boundaries and native high descriptors");
    close(source); unlink(a); unlink(b);
    puts("PASS native file identity");

    before = fd_count();
    for (i=0; i<4; ++i) CHECK(xvh_start_filter(bad,&input,&output)==-1);
    CHECK(fd_count()==before);
    pid = xvh_start_filter(cat,&input,&output); CHECK(pid > 0);
    CHECK(write(input,"filter",6)==6); close(input);
    FD_ZERO(&ready); FD_SET(output,&ready);
    count = select(output+1,(int *)&ready,NULL,NULL,&timeout);
    if (count <= 0) { kill(pid,SIGKILL); close(output); }
    CHECK(count > 0);
    count = read(output,buffer,sizeof(buffer)); close(output);
    CHECK(count==6 && memcmp(buffer,"filter",6)==0);
    CHECK(waitpid(pid,&status,0)==pid && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(fd_count()==before);
    puts("PASS native filter launch, EOF and failed-launch cleanup");

    memset(&timer,0,sizeof(timer));
    timer.it_value.tv_sec = -1;
    CHECK(xvh_setitimer(ITIMER_REAL,&timer,NULL)==-1 && errno==EINVAL);
    timer.it_value.tv_sec=0; timer.it_value.tv_usec=50000;
    signal(SIGALRM,alarmed);
    CHECK(xvh_setitimer(ITIMER_REAL,&timer,NULL)==0);
    timeout.tv_sec=2; timeout.tv_usec=0;
    CHECK(notify_select(0,NULL,NULL,NULL,&timeout)==-1 && errno==EINTR);
    CHECK(alarms==1);
    memset(&timer,0,sizeof(timer)); CHECK(xvh_setitimer(ITIMER_REAL,&timer,NULL)==0);
    puts("PASS notifier timer and public headers");
    return 0;
}
