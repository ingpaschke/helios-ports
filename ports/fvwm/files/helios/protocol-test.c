/* Native regression tests for the port's byte-stream and environment code. */
#include "platform.h"
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "fvwm/module.h"
extern int ReadFvwmPacket(int,unsigned long *,unsigned long **);
extern void SendText(int *,char *,unsigned long);
static int dead;
void DeadPipe(int unused) { dead++; }
#define CHECK(x) do { if (!(x)) { printf("FAIL line %d errno %d\n",__LINE__,errno); return 1; } } while (0)
int main(int argc,char **argv)
{
    int fds[2],out[2],status,error;
    unsigned long header[3],*body,frame[5]={START_FLAG,1,5,123,456};
    char bytes[64];pid_t child;
    struct hu_spawn_actions actions;
    struct hu_spawn_attr attr={HU_SPAWN_LOCAL,0,0,4};
    char *args[]={"fvwm-protocol-test","writer",0};
    if (argc>1 && !strcmp(argv[1],"writer")) {
        CHECK(ktm_write_full(3,"a",1)==1);
        {struct timeval tv={0,50000};select(0,0,0,0,&tv);}
        CHECK(ktm_write_full(3,"bc",2)==2);
        close(3);return 0;
    }
    CHECK(ktm_putenv("FVWM_TEST=first")==0);
    CHECK(ktm_putenv("FVWM_TEST=second")==0);
    CHECK(!strcmp(getenv("FVWM_TEST"),"second"));
    CHECK(ktm_putenv("FVWM_TEST_EXTRA=long appended environment value")==0);
    CHECK(!strcmp(getenv("FVWM_TEST_EXTRA"),"long appended environment value"));
    CHECK(!strcmp(getenv("FVWM_TEST"),"second"));
    CHECK(hu_channel(fds)==0);
    hu_spawn_actions_init(&actions);
    CHECK(hu_spawn_adddup2(&actions,fds[1],3)==0);
    if (argc>1) {attr.placement=HU_SPAWN_PROCESSOR;attr.processor=argv[1];}
    error=hu_spawn(&child,"/helios/local/libexec/fvwm-protocol-test",&actions,&attr,args,0);
    hu_spawn_actions_destroy(&actions);CHECK(error==0);close(fds[1]);
    CHECK(ktm_read_full(fds[0],bytes,3)==3 && !memcmp(bytes,"abc",3));
    CHECK(ktm_read_full(fds[0],bytes,1)==-1);close(fds[0]);
    CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    CHECK(hu_channel(fds)==0);
    CHECK(ktm_write_full(fds[1],frame,1)==1);
    CHECK(ktm_write_full(fds[1],((char *)frame)+1,sizeof(frame)-1)==sizeof(frame)-1);
    CHECK(ReadFvwmPacket(fds[0],header,&body)>0);
    CHECK(header[2]==5 && body[0]==123 && body[1]==456);free(body);
    out[0]=fds[1];out[1]=fds[0];SendText(out,"Desk 0 2",99);
    CHECK(ktm_read_full(fds[0],bytes,20)==20);
    CHECK(!memcmp(bytes+8,"Desk 0 2",8));
    frame[2]=2;CHECK(ktm_write_full(fds[1],frame,12)==12);
    CHECK(ReadFvwmPacket(fds[0],header,&body)<0 && dead==1 && !body);
    frame[2]=5;CHECK(ktm_write_full(fds[1],frame,13)==13);close(fds[1]);
    CHECK(ReadFvwmPacket(fds[0],header,&body)<0 && dead==2 && !body);close(fds[0]);
    CHECK(hu_channel(fds)==0);
    {fd_set reads,writes;struct timeval tv={0,0};FD_ZERO(&reads);FD_SET(fds[0],&reads);
     CHECK(ktm_select(fds[0]+1,&reads,0,0,&tv)==0);
     CHECK(ktm_write_full(fds[1],"x",1)==1);FD_SET(fds[0],&reads);tv.tv_sec=2;
     FD_ZERO(&writes);FD_SET(fds[1],&writes);
     CHECK(ktm_select(FD_SETSIZE,&reads,&writes,0,&tv)==2);
     CHECK(FD_ISSET(fds[0],&reads) && FD_ISSET(fds[1],&writes));
     CHECK(read(fds[0],bytes,1)==1 && bytes[0]=='x');
     FD_ZERO(&reads);FD_SET(fds[0],&reads);tv.tv_sec=0;tv.tv_usec=20000;
     CHECK(ktm_select(FD_SETSIZE,&reads,0,0,&tv)==0 && !FD_ISSET(fds[0],&reads));}
    close(fds[0]);close(fds[1]);
    printf("PASS fvwm environment, fragmented channels, packets, EOF, invalid frames, readiness%s\n",argc>1?" (remote child)":"");
    return 0;
}
