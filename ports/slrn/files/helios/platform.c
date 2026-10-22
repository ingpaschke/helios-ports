#include "platform.h"
#include <sys/wait.h>
int helios_system(const char *command)
{
    pid_t pid; int error,status;
    char *args[4];
    if(!command) return 1;
    args[0]="/helios/bin/shell";args[1]="-c";args[2]=(char *)command;args[3]=NULL;
    error=hu_spawn(&pid,args[0],NULL,NULL,args,environ);
    if(error){errno=error;return -1;}
    while(waitpid(pid,&status,0)<0) if(errno!=EINTR)return -1;
    return status;
}
FILE *helios_popen(const char *command,const char *mode)
{ (void)command;(void)mode;errno=ENOSYS;return NULL; }
int helios_pclose(FILE *stream)
{ (void)stream;errno=ENOSYS;return -1; }
int helios_no_fork(void){errno=ENOSYS;return -1;}

/* Helios Clib's string-stream formatter counts every character even after
 * _ocnt is exhausted. _flushbuf discards overflow for _IOSTRG. Starting
 * with _IOERR set prevents expected truncation becoming a negative return
 * in vfprintf; the result is the required (untruncated) character count.
 * This is specific to the native Clib FILE ABI, not a Unix FILE adapter. */
int vsnprintf(char *buffer,size_t size,const char *format,va_list args)
{
    FILE stream;char sink;int result;
    if(size>0x7fffffffU){errno=EINVAL;return -1;}
    memset(&stream,0,sizeof(stream));
    stream._flag=0x08|0x02|_IOERR; /* _IOSTRG | _IOWRITE | _IOERR */
    stream._ptr=(unsigned char *)(size?buffer:&sink);
    stream._ocnt=size?(int)size-1:0;
    result=vfprintf(&stream,format,args);
    if(size)*stream._ptr=0;
    return result;
}
