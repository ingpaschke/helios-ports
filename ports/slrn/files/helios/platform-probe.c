#include "platform.h"
static int lock_errors;
static void slrn_exit_error(char *format,...){(void)format;lock_errors++;}
#include "lock.inc"
static int format(char *out,unsigned size,const char *fmt,...)
{int n;va_list args;va_start(args,fmt);n=vsnprintf(out,size,fmt,args);va_end(args);return n;}
int main(void)
{
    char b[32],guard[5]={'a','b','c','d','e'};int n;FILE *f;
    n=format(b,sizeof(b),"%s %04d %.2f","test",7,1.25);
    if(n!=14 || strcmp(b,"test 0007 1.25")) {printf("format failure %d [%s]\n",n,b);return 1;}
    if(format(guard+1,3,"abcdef")!=6 || memcmp(guard,"aab\0e",5))return 2;
    if(format(NULL,0,"abc%d",42)!=5)return 3;
    if(format(guard,1,"test")!=4 || guard[0]!=0)return 4;
    if(helios_system("echo spawn-ok > /helios/tmp/slrn-spawn-result")!=0)return 5;
    f=fopen("/helios/tmp/slrn-spawn-result","r");if(!f)return 6;
    if(!fgets(b,sizeof(b),f) || strncmp(b,"spawn-ok",8))return 7;
    fclose(f);
    if(make_lock("/helios/tmp/slrn-lock-probe")<0)return 8;
    test_lock("/helios/tmp/slrn-lock-probe");if(lock_errors)return 9;
    f=fopen("/helios/tmp/slrn-lock-probe","w");if(!f)return 10;
    fputs("42\n",f);fclose(f);
    test_lock("/helios/tmp/slrn-lock-probe");if(lock_errors!=1)return 11;
    remove("/helios/tmp/slrn-lock-probe");
    puts("PASS bounded native formatting, spawn/wait, task-name and legacy PID lock handling");return 0;
}
